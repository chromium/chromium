// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/ukm_internals_ui.h"

#import "base/functional/bind.h"
#import "base/memory/ref_counted_memory.h"
#import "components/metrics_services_manager/metrics_services_manager.h"
#import "components/ukm/debug/ukm_debug_data_extractor.h"
#import "components/ukm/ukm_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/grit/ios_resources.h"
#import "ios/web/public/webui/url_data_source_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  UkmMessageHandler(const UkmMessageHandler&) = delete;
  UkmMessageHandler& operator=(const UkmMessageHandler&) = delete;

  ~UkmMessageHandler() override;

  // web::WebUIIOSMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleRequestUkmData(const base::Value::List& args);

  const ukm::UkmService* ukm_service_;
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
  web::WebUIIOSDataSource::Add(ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreateUkmInternalsUIHTMLSource());
}

UkmInternalsUI::~UkmInternalsUI() {}
