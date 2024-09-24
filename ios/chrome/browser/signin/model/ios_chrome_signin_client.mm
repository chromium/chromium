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
    ChromeBrowserState* browser_state,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    scoped_refptr<HostContentSettingsMap> host_content_settings_map)
    : network_callback_helper_(
          std::make_unique<WaitForNetworkCallbackHelperIOS>()),
      browser_state_(browser_state),
      cookie_settings_(cookie_settings),
      host_content_settings_map_(host_content_settings_map) {}

IOSChromeSigninClient::~IOSChromeSigninClient() {
}

void IOSChromeSigninClient::Shutdown() {
  network_callback_helper_.reset();
}

PrefService* IOSChromeSigninClient::GetPrefs() {
  return browser_state_->GetPrefs();
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromeSigninClient::GetURLLoaderFactory() {
  return browser_state_->GetSharedURLLoaderFactory();
}

network::mojom::CookieManager* IOSChromeSigninClient::GetCookieManager() {
  return browser_state_->GetCookieManager();
}

network::mojom::NetworkContext* IOSChromeSigninClient::GetNetworkContext() {
  return browser_state_->GetNetworkContext();
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
  return std::make_unique<GaiaAuthFetcherIOS>(
      consumer, source, GetURLLoaderFactory(), browser_state_);
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
      size_t groups_count = 0;
      size_t grouped_tabs_count = 0;

      BrowserList* browser_list =
          BrowserListFactory::GetForProfile(browser_state_);
      for (Browser* browser : browser_list->BrowsersOfType(
               BrowserList::BrowserType::kRegularAndInactive)) {
        WebStateList* web_state_list = browser->GetWebStateList();
        tabs_count += web_state_list->count();
        for (const TabGroup* group : web_state_list->GetGroups()) {
          ++groups_count;
          grouped_tabs_count += group->range().count();
        }
      }

      signin_metrics::RecordTabAndGroupCountsOnSignin(
          access_point, signin::ConsentLevel::kSignin, tabs_count, groups_count,
          grouped_tabs_count);
  }
}
