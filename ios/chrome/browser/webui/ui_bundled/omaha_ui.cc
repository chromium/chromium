// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webui/ui_bundled/omaha_ui.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "ios/chrome/browser/omaha/model/omaha_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
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
  void OnDebugInformationAvailable(const std::string& callback_id,
                                   base::Value::Dict debug_information);

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
  CHECK_EQ(1u, args.size());
  std::string callback_id = args[0].GetString();

  OmahaService::GetDebugInformation(
      base::BindOnce(&OmahaDOMHandler::OnDebugInformationAvailable,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void OmahaDOMHandler::OnDebugInformationAvailable(
    const std::string& callback_id,
    base::Value::Dict debug_information) {
  web_ui()->ResolveJavascriptCallback(base::Value(callback_id),
                                      /*response=*/debug_information);
}

}  // namespace

// OmahaUI
OmahaUI::OmahaUI(web::WebUIIOS* web_ui, const std::string& host)
    : WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<OmahaDOMHandler>());

  // Set up the chrome://omaha/ source.
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(profile, CreateOmahaUIHTMLSource());
}

OmahaUI::~OmahaUI() {}
