// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/local_state/local_state_ui.h"

#import <memory>

#import "base/bind.h"
#import "base/values.h"
#import "components/grit/dev_ui_components_resources.h"
#import "components/local_state/local_state_utils.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// UI Handler for chrome://local-state. Displays the Local State file as JSON.
class LocalStateUIHandler : public web::WebUIIOSMessageHandler {
 public:
  LocalStateUIHandler() = default;

  LocalStateUIHandler(const LocalStateUIHandler&) = delete;
  LocalStateUIHandler& operator=(const LocalStateUIHandler&) = delete;

  ~LocalStateUIHandler() override = default;

  // web::WebUIIOSMessageHandler:
  void RegisterMessages() override;

 private:
  // Called from JS when the page has loaded. Serializes local state prefs and
  // sends them to the page.
  void HandleRequestJson(const base::Value::List& args);
};

void LocalStateUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestJson",
      base::BindRepeating(&LocalStateUIHandler::HandleRequestJson,
                          base::Unretained(this)));
}

void LocalStateUIHandler::HandleRequestJson(const base::Value::List& args) {
  std::string json;
  if (!GetPrefsAsJson(GetApplicationContext()->GetLocalState(), &json))
    json = "Error loading Local State file.";

  const base::Value& callback_id = args[0];
  web_ui()->ResolveJavascriptCallback(callback_id, base::Value(json));
}

}  // namespace

LocalStateUI::LocalStateUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  // Set up the chrome://local-state source.
  web::WebUIIOSDataSource* html_source =
      web::WebUIIOSDataSource::Create(kChromeUILocalStateHost);
  html_source->SetDefaultResource(IDR_LOCAL_STATE_HTML);
  html_source->AddResourcePath("local_state.js", IDR_LOCAL_STATE_JS);
  web::WebUIIOSDataSource::Add(ChromeBrowserState::FromWebUIIOS(web_ui),
                               html_source);
  web_ui->AddMessageHandler(std::make_unique<LocalStateUIHandler>());
}

LocalStateUI::~LocalStateUI() {}
