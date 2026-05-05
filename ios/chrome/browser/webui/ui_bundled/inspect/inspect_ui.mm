// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/inspect/inspect_ui.h"

#import "base/functional/bind.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature_factory.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_message.h"
#import "ios/chrome/grit/inspect_resources.h"
#import "ios/chrome/grit/inspect_resources_map.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/webui/resource_path.h"

namespace {

// Used to record when the user loads the inspect page.
const char kInspectPageVisited[] = "IOSInspectPageVisited";

web::WebUIIOSDataSource* CreateInspectUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIInspectHost);

  source->AddLocalizedString("inspectConsoleNotice",
                             IDS_IOS_INSPECT_UI_CONSOLE_NOTICE);
  source->AddLocalizedString("inspectConsoleStartLogging",
                             IDS_IOS_INSPECT_UI_CONSOLE_START_LOGGING);
  source->AddLocalizedString("inspectConsoleStopLogging",
                             IDS_IOS_INSPECT_UI_CONSOLE_STOP_LOGGING);
  source->AddResourcePaths(base::span(kInspectResources));

  source->SetDefaultResource(IDR_INSPECT_INSPECT_HTML);
  return source;
}

// The handler for Javascript messages for the chrome://inspect/ page.
class InspectDOMHandler : public inspect::mojom::PageHandler,
                          public JavaScriptConsoleFeatureDelegate {
 public:
  InspectDOMHandler(JavaScriptConsoleFeature* feature,
                    mojo::PendingRemote<inspect::mojom::Page> page,
                    mojo::PendingReceiver<inspect::mojom::PageHandler> receiver)
      : feature_(feature),
        receiver_(this, std::move(receiver)),
        page_(std::move(page)) {}

  InspectDOMHandler(const InspectDOMHandler&) = delete;
  InspectDOMHandler& operator=(const InspectDOMHandler&) = delete;

  ~InspectDOMHandler() override { SetLoggingEnabled(false); }

  // inspect::mojom::PageHandler implementation.
  void SetLoggingEnabled(bool enabled) override {
    feature_->SetDelegate(enabled ? this : nullptr);
  }

  // JavaScriptConsoleFeatureDelegate implementation.
  void DidReceiveConsoleMessage(
      web::WebState* web_state,
      web::WebFrame* sender_frame,
      const JavaScriptConsoleMessage& message) override {
    if (!page_.is_bound()) {
      return;
    }

    std::string sender_frame_id = sender_frame->GetFrameId();
    web::WebFrame* main_web_frame =
        web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
    std::string main_web_frame_id =
        main_web_frame ? main_web_frame->GetFrameId() : sender_frame_id;

    std::string url = message.url.spec();
    std::string level = base::SysNSStringToUTF8(message.level);
    std::string js_message = base::SysNSStringToUTF8(message.message);

    page_->LogMessageReceived(main_web_frame_id, sender_frame_id, url, level,
                              js_message);
  }

 private:
  raw_ptr<JavaScriptConsoleFeature> feature_;

  // The receiver for the PageHandler interface.
  mojo::Receiver<inspect::mojom::PageHandler> receiver_;

  // The remote for the Page interface.
  mojo::Remote<inspect::mojom::Page> page_;
};

}  // namespace

InspectUI::InspectUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  base::RecordAction(base::UserMetricsAction(kInspectPageVisited));

  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(profile, CreateInspectUIHTMLSource());

  web_ui->GetWebState()->GetInterfaceBinderForMainFrame()->AddInterface(
      base::BindRepeating(&InspectUI::BindInterface, base::Unretained(this)));
}

InspectUI::~InspectUI() {
  web_ui()->GetWebState()->GetInterfaceBinderForMainFrame()->RemoveInterface(
      inspect::mojom::PageHandlerFactory::Name_);
}

void InspectUI::BindInterface(
    mojo::PendingReceiver<inspect::mojom::PageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void InspectUI::CreatePageHandler(
    mojo::PendingRemote<inspect::mojom::Page> page,
    mojo::PendingReceiver<inspect::mojom::PageHandler> handler) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  JavaScriptConsoleFeature* feature =
      JavaScriptConsoleFeatureFactory::GetForProfile(profile);
  handler_ = std::make_unique<InspectDOMHandler>(feature, std::move(page),
                                                 std::move(handler));
}
