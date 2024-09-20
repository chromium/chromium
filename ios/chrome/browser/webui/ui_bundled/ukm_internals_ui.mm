// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/ukm_internals_ui.h"

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted_memory.h"
#import "components/grit/ukm_resources.h"
#import "components/grit/ukm_resources_map.h"
#import "components/metrics_services_manager/metrics_services_manager.h"
#import "components/ukm/debug/ukm_debug_data_extractor.h"
#import "components/ukm/ukm_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/webui/url_data_source_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace {

web::WebUIIOSDataSource* CreateUkmInternalsUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIURLKeyedMetricsHost);

  source->AddResourcePaths(base::make_span(kUkmResources, kUkmResourcesSize));
  source->SetDefaultResource(IDR_UKM_UKM_INTERNALS_HTML);
  return source;
}

// The handler for Javascript messages for the chrome://ukm/ page.
class UkmMessageHandler : public web::WebUIIOSMessageHandler {
 public:
  explicit UkmMessageHandler(const ukm::UkmService* ukm_service);

  UkmMessageHandler(const UkmMessageHandler&) = delete;
  UkmMessageHandler& operator=(const UkmMessageHandler&) = delete;

  ~UkmMessageHandler() override;

  // web::WebUIIOSMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleRequestUkmData(const base::Value::List& args);

  raw_ptr<const ukm::UkmService> ukm_service_;
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

void UkmMessageHandler::HandleRequestUkmData(const base::Value::List& args) {
  base::Value ukm_debug_data =
      ukm::debug::UkmDebugDataExtractor::GetStructuredData(ukm_service_);

  std::string callback_id;
  if (!args.empty() && args[0].is_string())
    callback_id = args[0].GetString();

  web_ui()->ResolveJavascriptCallback(base::Value(callback_id),
                                      std::move(ukm_debug_data));
}

}  // namespace

// Changes to this class should be in sync with its non-iOS equivalent
// chrome/browser/ui/webui/ukm/ukm_internals_ui.cc
UkmInternalsUI::UkmInternalsUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ukm::UkmService* ukm_service =
      GetApplicationContext()->GetMetricsServicesManager()->GetUkmService();
  web_ui->AddMessageHandler(std::make_unique<UkmMessageHandler>(ukm_service));

  // Set up the chrome://ukm/ source.
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateUkmInternalsUIHTMLSource());
}

UkmInternalsUI::~UkmInternalsUI() {}
