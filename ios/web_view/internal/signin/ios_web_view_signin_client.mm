// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/signin/ios_web_view_signin_client.h"

#import "components/signin/core/browser/cookie_settings_util.h"
#import "components/signin/ios/browser/wait_for_network_callback_helper_ios.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "components/version_info/channel.h"
#import "ios/web_view/internal/signin/web_view_gaia_auth_fetcher.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

IOSWebViewSigninClient::IOSWebViewSigninClient(
    PrefService* pref_service,
    ios_web_view::WebViewBrowserState* browser_state)
    : network_callback_helper_(
          std::make_unique<WaitForNetworkCallbackHelperIOS>()),
      pref_service_(pref_service),
      browser_state_(browser_state) {}

IOSWebViewSigninClient::~IOSWebViewSigninClient() {
}

void IOSWebViewSigninClient::Shutdown() {
  network_callback_helper_.reset();
}

PrefService* IOSWebViewSigninClient::GetPrefs() {
  return pref_service_;
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSWebViewSigninClient::GetURLLoaderFactory() {
  return browser_state_->GetSharedURLLoaderFactory();
}

network::mojom::CookieManager* IOSWebViewSigninClient::GetCookieManager() {
  return browser_state_->GetCookieManager();
}

network::mojom::NetworkContext* IOSWebViewSigninClient::GetNetworkContext() {
  return browser_state_->GetNetworkContext();
}

void IOSWebViewSigninClient::DoFinalInit() {}

bool IOSWebViewSigninClient::AreSigninCookiesAllowed() {
  return false;
}

bool IOSWebViewSigninClient::AreSigninCookiesDeletedOnExit() {
  return false;
}

void IOSWebViewSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
  NOTIMPLEMENTED();
}

void IOSWebViewSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
  NOTIMPLEMENTED();
}

void IOSWebViewSigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric,
    bool has_sync_account) {
  std::move(on_signout_decision_reached).Run(SignoutDecision::ALLOW);
}

bool IOSWebViewSigninClient::AreNetworkCallsDelayed() {
  return network_callback_helper_->AreNetworkCallsDelayed();
}

void IOSWebViewSigninClient::DelayNetworkCall(base::OnceClosure callback) {
  network_callback_helper_->DelayNetworkCall(std::move(callback));
}

std::unique_ptr<GaiaAuthFetcher> IOSWebViewSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source) {
  return std::make_unique<ios_web_view::WebViewGaiaAuthFetcher>(
      consumer, source, GetURLLoaderFactory());
}

version_info::Channel IOSWebViewSigninClient::GetClientChannel() {
  // TODO(crbug.com/40216038): pass the correct channel information once
  // implemented.
  return version_info::Channel::STABLE;
}

void IOSWebViewSigninClient::OnPrimaryAccountChanged(
    signin::PrimaryAccountChangeEvent event_details) {}
