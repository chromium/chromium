// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webui/ui_bundled/ntp_tiles_internals_ui.h"

#include <memory>
#include <string_view>
#include <vector>

#include "components/grit/dev_ui_components_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler_client.h"
#include "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#include "ios/chrome/browser/ntp_tiles/model/ios_most_visited_sites_factory.h"
#include "ios/chrome/browser/ntp_tiles/model/ios_popular_sites_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace {

// The implementation for the chrome://ntp-tiles-internals page.
class IOSNTPTilesInternalsMessageHandlerBridge
    : public web::WebUIIOSMessageHandler,
      public ntp_tiles::NTPTilesInternalsMessageHandlerClient {
 public:
  // `favicon_service` must not be null and must outlive this object.
  explicit IOSNTPTilesInternalsMessageHandlerBridge(
      favicon::FaviconService* favicon_service)
      : handler_(favicon_service) {}

  IOSNTPTilesInternalsMessageHandlerBridge(
      const IOSNTPTilesInternalsMessageHandlerBridge&) = delete;
  IOSNTPTilesInternalsMessageHandlerBridge& operator=(
      const IOSNTPTilesInternalsMessageHandlerBridge&) = delete;

 private:
  // web::WebUIIOSMessageHandler:
  void RegisterMessages() override;

  // ntp_tiles::NTPTilesInternalsMessageHandlerClient
  bool SupportsNTPTiles() override;
  std::unique_ptr<ntp_tiles::MostVisitedSites> MakeMostVisitedSites() override;
  PrefService* GetPrefs() override;
  using MessageCallback =
      base::RepeatingCallback<void(const base::Value::List&)>;
  void RegisterMessageCallback(std::string_view message,
                               MessageCallback callback) override;
  void CallJavascriptFunctionSpan(
      std::string_view name,
      base::span<const base::ValueView> values) override;

  ntp_tiles::NTPTilesInternalsMessageHandler handler_;
};

void IOSNTPTilesInternalsMessageHandlerBridge::RegisterMessages() {
  handler_.RegisterMessages(this);
}

bool IOSNTPTilesInternalsMessageHandlerBridge::SupportsNTPTiles() {
  return !ProfileIOS::FromWebUIIOS(web_ui())->IsOffTheRecord();
}

std::unique_ptr<ntp_tiles::MostVisitedSites>
IOSNTPTilesInternalsMessageHandlerBridge::MakeMostVisitedSites() {
  return IOSMostVisitedSitesFactory::NewForBrowserState(
      ProfileIOS::FromWebUIIOS(web_ui()));
}

PrefService* IOSNTPTilesInternalsMessageHandlerBridge::GetPrefs() {
  return ProfileIOS::FromWebUIIOS(web_ui())->GetPrefs();
}

void IOSNTPTilesInternalsMessageHandlerBridge::RegisterMessageCallback(
    std::string_view message,
    MessageCallback callback) {
  web_ui()->RegisterMessageCallback(message, std::move(callback));
}

void IOSNTPTilesInternalsMessageHandlerBridge::CallJavascriptFunctionSpan(
    std::string_view name,
    base::span<const base::ValueView> values) {
  web_ui()->CallJavascriptFunction(name, values);
}

web::WebUIIOSDataSource* CreateNTPTilesInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUINTPTilesInternalsHost);

  source->AddResourcePath("ntp_tiles_internals.js", IDR_NTP_TILES_INTERNALS_JS);
  source->AddResourcePath("ntp_tiles_internals.css",
                          IDR_NTP_TILES_INTERNALS_CSS);
  source->SetDefaultResource(IDR_NTP_TILES_INTERNALS_HTML);
  return source;
}

}  // namespace

NTPTilesInternalsUI::NTPTilesInternalsUI(web::WebUIIOS* web_ui,
                                         const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(profile, CreateNTPTilesInternalsHTMLSource());
  web_ui->AddMessageHandler(
      std::make_unique<IOSNTPTilesInternalsMessageHandlerBridge>(
          ios::FaviconServiceFactory::GetForBrowserState(
              profile, ServiceAccessType::EXPLICIT_ACCESS)));
}

NTPTilesInternalsUI::~NTPTilesInternalsUI() {}
