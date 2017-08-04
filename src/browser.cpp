// Based on the CEF demo library
#include "browser.h"

#include <X11/Xlib.h>
#include <iostream>
#include "common.h"
#include "include/base/cef_bind.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

static int XErrorHandlerImpl(Display* display, XErrorEvent* event) {
    LOG(WARNING) << "X error received: "
                 << "type " << event->type << ", "
                 << "serial " << event->serial << ", "
                 << "error_code " << static_cast<int>(event->error_code) << ", "
                 << "request_code " << static_cast<int>(event->request_code)
                 << ", "
                 << "minor_code " << static_cast<int>(event->minor_code);
    return 0;
}

static int XIOErrorHandlerImpl(Display* display) {
    return 0;
}

CefMainArgs XboxLoginBrowserApp::cefMainArgs;

void XboxLoginBrowserApp::openBrowser() {
    XSetErrorHandler(XErrorHandlerImpl);
    XSetIOErrorHandler(XIOErrorHandlerImpl);

    CefRefPtr<XboxLoginBrowserApp> app (new XboxLoginBrowserApp());

    CefSettings settings;
    settings.single_process = true;
    settings.no_sandbox = true;
    CefString(&settings.resources_dir_path) = getCWD() + "libs/cef/";
    CefString(&settings.locales_dir_path) = getCWD() + "libs/cef/locales/";
    CefInitialize(cefMainArgs, settings, app.get(), nullptr);
    CefRunMessageLoop();
    CefShutdown();
}

XboxLoginBrowserApp::XboxLoginBrowserApp() : externalInterfaceHandler(new XboxLiveV8Handler()) {

}

void XboxLoginBrowserApp::OnContextInitialized() {
    CefWindowInfo window;
    window.width = 480;
    window.height = 640;
    CefRefPtr<SimpleHandler> handler(new SimpleHandler());
    CefBrowserSettings browserSettings;

    CefBrowserHost::CreateBrowser(window, handler,
                                  "https://login.live.com/ppsecure/InlineConnect.srf?id=80604&platform=android2.1.0504.0524&client_id=android-app%3A%2F%2Fcom.mojang.minecraftpe.H62DKCBHJP6WXXIV7RBFOGOL4NAK4E6Y&cobrandid=90023&mkt=en-US&phone=&email=",
                                  browserSettings, NULL);
}

void XboxLoginBrowserApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                           CefRefPtr<CefV8Context> context) {
    printf("OnContextCreated\n");
    CefRefPtr<CefV8Value> global = context->GetGlobal();
    CefRefPtr<CefV8Value> object = CefV8Value::CreateObject(nullptr, nullptr);
    object->SetValue("Property", CefV8Value::CreateFunction("Property", externalInterfaceHandler), V8_PROPERTY_ATTRIBUTE_NONE);
    object->SetValue("FinalBack", CefV8Value::CreateFunction("FinalBack", externalInterfaceHandler), V8_PROPERTY_ATTRIBUTE_NONE);
    object->SetValue("FinalNext", CefV8Value::CreateFunction("FinalNext", externalInterfaceHandler), V8_PROPERTY_ATTRIBUTE_NONE);
    global->SetValue("external", object, V8_PROPERTY_ATTRIBUTE_NONE);
}

void SimpleHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    CEF_REQUIRE_UI_THREAD();
}

void SimpleHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    browserList.push_back(browser);
}

void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    for (auto bit = browserList.begin(); bit != browserList.end(); ++bit) {
        if ((*bit)->IsSame(browser)) {
            browserList.erase(bit);
            break;
        }
    }

    if (browserList.empty()) {
        // All browser windows have closed. Quit the application message loop.
        CefQuitMessageLoop();
    }
}

void SimpleHandler::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode,
                                const CefString& errorText, const CefString& failedUrl) {
    CEF_REQUIRE_UI_THREAD();
    if (errorCode == ERR_ABORTED)
        return;

    std::stringstream ss;
    ss << "<html><body><h2>Failed to load URL " << std::string(failedUrl) << "</h2>"
       << "<p>" << std::string(errorText) << " (" << errorCode << ").</p></body></html>";
    frame->LoadString(ss.str(), failedUrl);
}

void SimpleHandler::CloseAllBrowsers(bool forceClose) {
    if (!CefCurrentlyOn(TID_UI)) {
        CefPostTask(TID_UI, base::Bind(&SimpleHandler::CloseAllBrowsers, this, forceClose));
        return;
    }
    for (auto it = browserList.begin(); it != browserList.end(); ++it)
        (*it)->GetHost()->CloseBrowser(forceClose);
}

bool XboxLiveV8Handler::Execute(const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& args,
                                CefRefPtr<CefV8Value>& retval, CefString& exception) {
    if (name == "Property") {
        if (args.size() == 1 && args[0]->IsString()) {
            CefString prop = args[0]->GetStringValue();
            printf("Get Property %s\n", prop.ToString().c_str());
            retval = CefV8Value::CreateString(properties[prop]);
            return true;
        } else if (args.size() == 2 && args[0]->IsString() && args[1]->IsString()) {
            CefString prop = args[0]->GetStringValue();
            CefString val = args[1]->GetStringValue();
            printf("Set Property %s = %s\n", prop.ToString().c_str(), val.ToString().c_str());
            properties[prop] = val;
            return true;
        }
    }
    printf("Invalid Execute: %s\n", name.ToString().c_str());
    return false;
}