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
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature_delegate.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature_factory.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_message.h"
#import "ios/chrome/grit/ios_resources.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"

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
  source->UseStringsJs();
  source->AddResourcePath("inspect.js", IDR_IOS_INSPECT_JS);
  source->SetDefaultResource(IDR_IOS_INSPECT_HTML);
  return source;
}

// The handler for Javascript messages for the chrome://inspect/ page.
class InspectDOMHandler : public web::WebUIIOSMessageHandler,
                          public JavaScriptConsoleFeatureDelegate {
 public:
  InspectDOMHandler();

  InspectDOMHandler(const InspectDOMHandler&) = delete;
  InspectDOMHandler& operator=(const InspectDOMHandler&) = delete;

  ~InspectDOMHandler() override;

  // WebUIIOSMessageHandler implementation
  void RegisterMessages() override;

  // JavaScriptConsoleFeatureDelegate
  void DidReceiveConsoleMessage(
      web::WebState* web_state,
      web::WebFrame* sender_frame,
      const JavaScriptConsoleMessage& message) override;

 private:
  // Handles the message from JavaScript to enable or disable console logging.
  void HandleSetLoggingEnabled(const base::Value::List& args);

  // Enables or disables console logging.
  void SetLoggingEnabled(bool enabled);

  // Whether or not logging is enabled.
  bool logging_enabled_ = false;
};

InspectDOMHandler::InspectDOMHandler() {}

InspectDOMHandler::~InspectDOMHandler() {
  // Clear delegate from WebStates.
  SetLoggingEnabled(false);
}

void InspectDOMHandler::HandleSetLoggingEnabled(const base::Value::List& args) {
  if (args.size() != 1) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  bool enabled = false;
  if (args[0].is_bool()) {
    enabled = args[0].GetBool();
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  SetLoggingEnabled(enabled);
}

void InspectDOMHandler::SetLoggingEnabled(bool enabled) {
  if (logging_enabled_ == enabled) {
    return;
  }

  logging_enabled_ = enabled;

  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  JavaScriptConsoleFeature* feature =
      JavaScriptConsoleFeatureFactory::GetForProfile(profile);

  feature->SetDelegate(enabled ? this : nullptr);
}

void InspectDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setLoggingEnabled",
      base::BindRepeating(&InspectDOMHandler::HandleSetLoggingEnabled,
                          base::Unretained(this)));
}

void InspectDOMHandler::DidReceiveConsoleMessage(
    web::WebState* web_state,
    web::WebFrame* sender_frame,
    const JavaScriptConsoleMessage& message) {
  web::WebFrame* inspect_ui_main_frame = web_ui()
                                             ->GetWebState()
                                             ->GetPageWorldWebFramesManager()
                                             ->GetMainWebFrame();
  if (!inspect_ui_main_frame) {
    // Disable logging and drop this message because the inspect page no longer
    // exists.
    SetLoggingEnabled(false);
    return;
  }

  std::string sender_frame_id = sender_frame->GetFrameId();
  web::WebFrame* main_web_frame =
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
  std::string main_web_frame_id =
      main_web_frame ? main_web_frame->GetFrameId() : sender_frame_id;

  auto params = base::Value::List()
                    .Append(main_web_frame_id)
                    .Append(sender_frame_id)
                    .Append(message.url.spec())
                    .Append(base::SysNSStringToUTF8(message.level))
                    .Append(base::SysNSStringToUTF8(message.message));
  inspect_ui_main_frame->CallJavaScriptFunction(
      "inspectWebUI.logMessageReceived", params);
}

}  // namespace

InspectUI::InspectUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  base::RecordAction(base::UserMetricsAction(kInspectPageVisited));

  web_ui->AddMessageHandler(std::make_unique<InspectDOMHandler>());

  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateInspectUIHTMLSource());
}

InspectUI::~InspectUI() {}
