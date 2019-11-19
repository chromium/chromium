// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/signin/ios/browser/wait_for_network_callback_helper.h"
#include "components/signin/public/base/signin_client.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ios_web_view {
class WebViewBrowserState;
}

// iOS WebView specific signin client.
class IOSWebViewSigninClient : public SigninClient {
 public:
  IOSWebViewSigninClient(
      PrefService* pref_service,
      ios_web_view::WebViewBrowserState* browser_state,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<HostContentSettingsMap> host_content_settings_map);

  ~IOSWebViewSigninClient() override;

  // KeyedService implementation.
  void Shutdown() override;

  // SigninClient implementation.
  PrefService* GetPrefs() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::CookieManager* GetCookieManager() override;
  void DoFinalInit() override;
  bool AreSigninCookiesAllowed() override;
  bool AreSigninCookiesDeletedOnExit() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric) override;
  void DelayNetworkCall(base::OnceClosure callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) override;

 private:
  // Helper to delay callbacks until connection becomes online again.
  std::unique_ptr<WaitForNetworkCallbackHelper> network_callback_helper_;
  // The PrefService associated with this service.
  PrefService* pref_service_;
  ios_web_view::WebViewBrowserState* browser_state_;
  // Used to check if sign in cookies are allowed.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // Used to add and remove content settings observers.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  DISALLOW_COPY_AND_ASSIGN(IOSWebViewSigninClient);
};

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_
