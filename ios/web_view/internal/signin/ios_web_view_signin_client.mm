// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"

#include "base/macros.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#include "ios/web_view/internal/signin/web_view_gaia_auth_fetcher.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSWebViewSigninClient::IOSWebViewSigninClient(
    PrefService* pref_service,
    ios_web_view::WebViewBrowserState* browser_state)
    : network_callback_helper_(
          std::make_unique<WaitForNetworkCallbackHelper>()),
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
    signin_metrics::ProfileSignout signout_source_metric) {
  std::move(on_signout_decision_reached).Run(SignoutDecision::ALLOW_SIGNOUT);
}

void IOSWebViewSigninClient::DelayNetworkCall(base::OnceClosure callback) {
  network_callback_helper_->HandleCallback(std::move(callback));
}

std::unique_ptr<GaiaAuthFetcher> IOSWebViewSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source) {
  return std::make_unique<ios_web_view::WebViewGaiaAuthFetcher>(
      consumer, source, GetURLLoaderFactory());
}

