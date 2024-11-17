// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/ios_chrome_signin_client.h"

#import "base/strings/utf_string_conversions.h"
#import "components/metrics/metrics_service.h"
#import "components/signin/core/browser/cookie_settings_util.h"
#import "components/signin/ios/browser/wait_for_network_callback_helper_ios.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/signin/model/gaia_auth_fetcher_ios.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

IOSChromeSigninClient::IOSChromeSigninClient(
    ProfileIOS* profile,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    scoped_refptr<HostContentSettingsMap> host_content_settings_map)
    : network_callback_helper_(
          std::make_unique<WaitForNetworkCallbackHelperIOS>()),
      profile_(profile),
      cookie_settings_(cookie_settings),
      host_content_settings_map_(host_content_settings_map) {}

IOSChromeSigninClient::~IOSChromeSigninClient() {
}

void IOSChromeSigninClient::Shutdown() {
  network_callback_helper_.reset();
}

PrefService* IOSChromeSigninClient::GetPrefs() {
  return profile_->GetPrefs();
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromeSigninClient::GetURLLoaderFactory() {
  return profile_->GetSharedURLLoaderFactory();
}

network::mojom::CookieManager* IOSChromeSigninClient::GetCookieManager() {
  return profile_->GetCookieManager();
}

network::mojom::NetworkContext* IOSChromeSigninClient::GetNetworkContext() {
  return profile_->GetNetworkContext();
}

void IOSChromeSigninClient::DoFinalInit() {}

bool IOSChromeSigninClient::AreSigninCookiesAllowed() {
  return signin::SettingsAllowSigninCookies(cookie_settings_.get());
}

bool IOSChromeSigninClient::AreSigninCookiesDeletedOnExit() {
  return signin::SettingsDeleteSigninCookiesOnExit(cookie_settings_.get());
}

void IOSChromeSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
  host_content_settings_map_->AddObserver(observer);
}

void IOSChromeSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
  host_content_settings_map_->RemoveObserver(observer);
}

bool IOSChromeSigninClient::AreNetworkCallsDelayed() {
  return network_callback_helper_->AreNetworkCallsDelayed();
}

void IOSChromeSigninClient::DelayNetworkCall(base::OnceClosure callback) {
  network_callback_helper_->DelayNetworkCall(std::move(callback));
}

std::unique_ptr<GaiaAuthFetcher> IOSChromeSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source) {
  return std::make_unique<GaiaAuthFetcherIOS>(consumer, source,
                                              GetURLLoaderFactory(), profile_);
}

version_info::Channel IOSChromeSigninClient::GetClientChannel() {
  return GetChannel();
}

void IOSChromeSigninClient::OnPrimaryAccountChanged(
    signin::PrimaryAccountChangeEvent event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      break;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      CHECK(event_details.GetSetPrimaryAccountAccessPoint().has_value());
      signin_metrics::AccessPoint access_point =
          event_details.GetSetPrimaryAccountAccessPoint().value();

      size_t tabs_count = 0;

      BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
      for (Browser* browser : browser_list->BrowsersOfType(
               BrowserList::BrowserType::kRegularAndInactive)) {
        tabs_count += browser->GetWebStateList()->count();
      }

      signin_metrics::RecordOpenTabCountOnSignin(
          access_point, signin::ConsentLevel::kSignin, tabs_count);
  }
}
