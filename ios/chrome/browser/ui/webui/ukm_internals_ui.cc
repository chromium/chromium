// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/ukm_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/ukm/debug/ukm_debug_data_extractor.h"
#include "components/ukm/ukm_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/grit/ios_resources.h"
#include "ios/web/public/webui/url_data_source_ios.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace {

web::WebUIIOSDataSource* CreateUkmInternalsUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIURLKeyedMetricsHost);

  source->AddResourcePath("ukm_internals.js", IDR_IOS_UKM_INTERNALS_JS);
  source->SetDefaultResource(IDR_IOS_UKM_INTERNALS_HTML);
  return source;
}

// The handler for Javascript messages for the chrome://ukm/ page.
class UkmMessageHandler : public web::WebUIIOSMessageHandler {
 public:
  explicit UkmMessageHandler(const ukm::UkmService* ukm_service);
  ~UkmMessageHandler() override;

  // web::WebUIIOSMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleRequestUkmData(const base::ListValue* args);

  const ukm::UkmService* ukm_service_;

  DISALLOW_COPY_AND_ASSIGN(UkmMessageHandler);
};

UkmMessageHandler::UkmMessageHandler(const ukm::UkmService* ukm_service)
    : ukm_service_(ukm_service) {}

UkmMessageHandler::~UkmMessageHandler() {}

void UkmMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestUkmData",
      base::BindRepeating(&UkmMessageHandler::HandleRequestUkmData,
                          base::Unretained(this)));
}

void UkmMessageHandler::HandleRequestUkmData(const base::ListValue* args) {
  base::Value ukm_debug_data =
      ukm::debug::UkmDebugDataExtractor::GetStructuredData(ukm_service_);

  std::string callback_id;
  args->GetString(0, &callback_id);
  web_ui()->ResolveJavascriptCallback(base::Value(callback_id),
                                      std::move(ukm_debug_data));
}

}  // namespace

// Changes to this class should be in sync with its non-iOS equivalent
// chrome/browser/ui/webui/ukm/ukm_internals_ui.cc
UkmInternalsUI::UkmInternalsUI(web::WebUIIOS* web_ui)
    : web::WebUIIOSController(web_ui) {
  ukm::UkmService* ukm_service =
      GetApplicationContext()->GetMetricsServicesManager()->GetUkmService();
  web_ui->AddMessageHandler(std::make_unique<UkmMessageHandler>(ukm_service));

  // Set up the chrome://ukm/ source.
  web::WebUIIOSDataSource::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreateUkmInternalsUIHTMLSource());
}

UkmInternalsUI::~UkmInternalsUI() {}
