// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/omaha_ui.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/omaha/omaha_service.h"
#include "ios/chrome/browser/url/chrome_url_constants.h"
#include "ios/chrome/grit/ios_resources.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

using web::WebUIIOSMessageHandler;

namespace {

web::WebUIIOSDataSource* CreateOmahaUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIOmahaHost);

  source->UseStringsJs();
  source->AddResourcePath("omaha.js", IDR_IOS_OMAHA_JS);
  source->SetDefaultResource(IDR_IOS_OMAHA_HTML);
  return source;
}

// OmahaDOMHandler

// The handler for Javascript messages for the chrome://omaha/ page.
class OmahaDOMHandler : public WebUIIOSMessageHandler {
 public:
  OmahaDOMHandler();

  OmahaDOMHandler(const OmahaDOMHandler&) = delete;
  OmahaDOMHandler& operator=(const OmahaDOMHandler&) = delete;

  ~OmahaDOMHandler() override;

  // WebUIIOSMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Asynchronously fetches the debug information. Called from JS.
  void HandleRequestDebugInformation(const base::Value::List& args);

  // Called when the debug information have been computed.
  void OnDebugInformationAvailable(base::Value::Dict debug_information);

  // WeakPtr factory needed because this object might be deleted before
  // receiving the callbacks from the OmahaService.
  base::WeakPtrFactory<OmahaDOMHandler> weak_ptr_factory_;
};

OmahaDOMHandler::OmahaDOMHandler() : weak_ptr_factory_(this) {}

OmahaDOMHandler::~OmahaDOMHandler() {}

void OmahaDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestOmahaDebugInformation",
      base::BindRepeating(&OmahaDOMHandler::HandleRequestDebugInformation,
                          base::Unretained(this)));
}

void OmahaDOMHandler::HandleRequestDebugInformation(
    const base::Value::List& args) {
  OmahaService::GetDebugInformation(
      base::BindOnce(&OmahaDOMHandler::OnDebugInformationAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OmahaDOMHandler::OnDebugInformationAvailable(
    base::Value::Dict debug_information) {
  base::ValueView args[] = {debug_information};
  web_ui()->CallJavascriptFunction("updateOmahaDebugInformation", args);
}

}  // namespace

// OmahaUI
OmahaUI::OmahaUI(web::WebUIIOS* web_ui, const std::string& host)
    : WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<OmahaDOMHandler>());

  // Set up the chrome://omaha/ source.
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(browser_state, CreateOmahaUIHTMLSource());
}

OmahaUI::~OmahaUI() {}
