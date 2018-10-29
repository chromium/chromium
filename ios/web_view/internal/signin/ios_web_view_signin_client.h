// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/ios/browser/wait_for_network_callback_helper.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

@class CWVSyncController;

// iOS WebView specific signin client.
class IOSWebViewSigninClient : public SigninClient,
                               public SigninErrorController::Observer {
 public:
  IOSWebViewSigninClient(
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::mojom::CookieManager* cookie_manager,
      SigninErrorController* signin_error_controller,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<HostContentSettingsMap> host_content_settings_map);

  ~IOSWebViewSigninClient() override;

  // KeyedService implementation.
  void Shutdown() override;

  // SigninClient implementation.
  std::string GetProductVersion() override;
  base::Time GetInstallDate() override;
  PrefService* GetPrefs() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::CookieManager* GetCookieManager() override;
  void DoFinalInit() override;
  bool IsFirstRun() const override;
  bool AreSigninCookiesAllowed() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric) override;
  void DelayNetworkCall(const base::Closure& callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;
  void OnSignedOut() override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

  // CWVSyncController setter/getter.
  void SetSyncController(CWVSyncController* sync_controller);
  CWVSyncController* GetSyncController() const;

 private:
  // Helper to delay callbacks until connection becomes online again.
  std::unique_ptr<WaitForNetworkCallbackHelper> network_callback_helper_;
  // The PrefService associated with this service.
  PrefService* pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::mojom::CookieManager* cookie_manager_;
  // Used to check for errors related to signing in.
  SigninErrorController* signin_error_controller_;
  // Used to check if sign in cookies are allowed.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // Used to add and remove content settings observers.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  // Used by WebViewProfileOAuth2TokenServiceIOSProviderImpl to fetch access
  // tokens. Also used to notify of signout events. Held weak so this class
  // does not determine |sync_controller_|'s lifetime.
  __weak CWVSyncController* sync_controller_ = nil;

  DISALLOW_COPY_AND_ASSIGN(IOSWebViewSigninClient);
};

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_
