// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_

#include <memory>

#include "components/signin/ios/browser/wait_for_network_callback_helper.h"
#include "components/signin/public/base/signin_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ios_web_view {
class WebViewBrowserState;
}

// iOS WebView specific signin client.
class IOSWebViewSigninClient : public SigninClient {
 public:
  IOSWebViewSigninClient(PrefService* pref_service,
                         ios_web_view::WebViewBrowserState* browser_state);

  IOSWebViewSigninClient(const IOSWebViewSigninClient&) = delete;
  IOSWebViewSigninClient& operator=(const IOSWebViewSigninClient&) = delete;

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
      signin_metrics::ProfileSignout signout_source_metric,
      bool has_sync_account) override;
  void DelayNetworkCall(base::OnceClosure callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) override;

 private:
  // Helper to delay callbacks until connection becomes online again.
  std::unique_ptr<WaitForNetworkCallbackHelper> network_callback_helper_;
  // The PrefService associated with this service.
  PrefService* pref_service_;
  // The browser_state_ associated with this service.
  ios_web_view::WebViewBrowserState* browser_state_;
};

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_
