// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"

#include "components/signin/core/browser/cookie_settings_util.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSWebViewSigninClient::IOSWebViewSigninClient(
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::mojom::CookieManager* cookie_manager,
    SigninErrorController* signin_error_controller,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    scoped_refptr<HostContentSettingsMap> host_content_settings_map)
    : network_callback_helper_(
          std::make_unique<WaitForNetworkCallbackHelper>()),
      pref_service_(pref_service),
      url_loader_factory_(url_loader_factory),
      cookie_manager_(cookie_manager),
      signin_error_controller_(signin_error_controller),
      cookie_settings_(cookie_settings),
      host_content_settings_map_(host_content_settings_map) {
  signin_error_controller_->AddObserver(this);
}

IOSWebViewSigninClient::~IOSWebViewSigninClient() {
  signin_error_controller_->RemoveObserver(this);
}

void IOSWebViewSigninClient::Shutdown() {
  network_callback_helper_.reset();
}

void IOSWebViewSigninClient::OnSignedOut() {}

std::string IOSWebViewSigninClient::GetProductVersion() {
  // TODO(crbug.com/768689): Implement this method with appropriate values.
  return "";
}

base::Time IOSWebViewSigninClient::GetInstallDate() {
  // TODO(crbug.com/768689): Implement this method with appropriate values.
  return base::Time::FromTimeT(0);
}

PrefService* IOSWebViewSigninClient::GetPrefs() {
  return pref_service_;
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSWebViewSigninClient::GetURLLoaderFactory() {
  return url_loader_factory_;
}

network::mojom::CookieManager* IOSWebViewSigninClient::GetCookieManager() {
  return cookie_manager_;
}

void IOSWebViewSigninClient::DoFinalInit() {}

bool IOSWebViewSigninClient::IsFirstRun() const {
  return false;
}

bool IOSWebViewSigninClient::AreSigninCookiesAllowed() {
  return signin::SettingsAllowSigninCookies(cookie_settings_.get());
}

void IOSWebViewSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
  host_content_settings_map_->AddObserver(observer);
}

void IOSWebViewSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
  host_content_settings_map_->RemoveObserver(observer);
}

void IOSWebViewSigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric) {
  std::move(on_signout_decision_reached).Run(SignoutDecision::ALLOW_SIGNOUT);
  [sync_controller_ didSignoutWithSourceMetric:signout_source_metric];
}

void IOSWebViewSigninClient::DelayNetworkCall(const base::Closure& callback) {
  network_callback_helper_->HandleCallback(callback);
}

std::unique_ptr<GaiaAuthFetcher> IOSWebViewSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    const std::string& source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<GaiaAuthFetcher>(consumer, source,
                                           url_loader_factory);
}

void IOSWebViewSigninClient::OnErrorChanged() {
  [sync_controller_ didUpdateAuthError:signin_error_controller_->auth_error()];
}

void IOSWebViewSigninClient::SetSyncController(
    CWVSyncController* sync_controller) {
  sync_controller_ = sync_controller;
}

CWVSyncController* IOSWebViewSigninClient::GetSyncController() const {
  return sync_controller_;
}
