// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/ntp_tiles_internals_ui.h"

#include <memory>

#include "components/grit/components_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler_client.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#include "ios/chrome/browser/ntp_tiles/ios_popular_sites_factory.h"
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
  // |favicon_service| must not be null and must outlive this object.
  explicit IOSNTPTilesInternalsMessageHandlerBridge(
      favicon::FaviconService* favicon_service)
      : handler_(favicon_service) {}

 private:
  // web::WebUIIOSMessageHandler:
  void RegisterMessages() override;

  // ntp_tiles::NTPTilesInternalsMessageHandlerClient
  bool SupportsNTPTiles() override;
  bool DoesSourceExist(ntp_tiles::TileSource source) override;
  std::unique_ptr<ntp_tiles::MostVisitedSites> MakeMostVisitedSites() override;
  PrefService* GetPrefs() override;
  void RegisterMessageCallback(
      const std::string& message,
      const base::RepeatingCallback<void(const base::ListValue*)>& callback)
      override;
  void CallJavascriptFunctionVector(
      const std::string& name,
      const std::vector<const base::Value*>& values) override;

  ntp_tiles::NTPTilesInternalsMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(IOSNTPTilesInternalsMessageHandlerBridge);
};

void IOSNTPTilesInternalsMessageHandlerBridge::RegisterMessages() {
  handler_.RegisterMessages(this);
}

bool IOSNTPTilesInternalsMessageHandlerBridge::SupportsNTPTiles() {
  return !ios::ChromeBrowserState::FromWebUIIOS(web_ui())->IsOffTheRecord();
}

bool IOSNTPTilesInternalsMessageHandlerBridge::DoesSourceExist(
    ntp_tiles::TileSource source) {
  switch (source) {
    case ntp_tiles::TileSource::TOP_SITES:
    case ntp_tiles::TileSource::SUGGESTIONS_SERVICE:
    case ntp_tiles::TileSource::POPULAR:
    case ntp_tiles::TileSource::POPULAR_BAKED_IN:
    case ntp_tiles::TileSource::HOMEPAGE:
      return true;
    case ntp_tiles::TileSource::CUSTOM_LINKS:
    case ntp_tiles::TileSource::WHITELIST:
    case ntp_tiles::TileSource::EXPLORE:
      return false;
  }
  NOTREACHED();
  return false;
}

std::unique_ptr<ntp_tiles::MostVisitedSites>
IOSNTPTilesInternalsMessageHandlerBridge::MakeMostVisitedSites() {
  return IOSMostVisitedSitesFactory::NewForBrowserState(
      ios::ChromeBrowserState::FromWebUIIOS(web_ui()));
}

PrefService* IOSNTPTilesInternalsMessageHandlerBridge::GetPrefs() {
  return ios::ChromeBrowserState::FromWebUIIOS(web_ui())->GetPrefs();
}

void IOSNTPTilesInternalsMessageHandlerBridge::RegisterMessageCallback(
    const std::string& message,
    const base::RepeatingCallback<void(const base::ListValue*)>& callback) {
  web_ui()->RegisterMessageCallback(message, callback);
}

void IOSNTPTilesInternalsMessageHandlerBridge::CallJavascriptFunctionVector(
    const std::string& name,
    const std::vector<const base::Value*>& values) {
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

NTPTilesInternalsUI::NTPTilesInternalsUI(web::WebUIIOS* web_ui)
    : web::WebUIIOSController(web_ui) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(browser_state,
                               CreateNTPTilesInternalsHTMLSource());
  web_ui->AddMessageHandler(
      std::make_unique<IOSNTPTilesInternalsMessageHandlerBridge>(
          ios::FaviconServiceFactory::GetForBrowserState(
              browser_state, ServiceAccessType::EXPLICIT_ACCESS)));
}

NTPTilesInternalsUI::~NTPTilesInternalsUI() {}
