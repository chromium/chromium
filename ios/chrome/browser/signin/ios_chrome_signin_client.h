// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_IOS_CHROME_SIGNIN_CLIENT_H_
#define IOS_CHROME_BROWSER_SIGNIN_IOS_CHROME_SIGNIN_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/signin/ios/browser/wait_for_network_callback_helper.h"
#include "components/signin/public/base/signin_client.h"
#include "net/cookies/cookie_change_dispatcher.h"

namespace ios {
class ChromeBrowserState;
}

// Concrete implementation of SigninClient for //ios/chrome.
class IOSChromeSigninClient : public SigninClient {
 public:
  IOSChromeSigninClient(
      ios::ChromeBrowserState* browser_state,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<HostContentSettingsMap> host_content_settings_map);
  ~IOSChromeSigninClient() override;

  // KeyedService implementation.
  void Shutdown() override;

  // SigninClient implementation.
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) override;
  void PreGaiaLogout(base::OnceClosure callback) override;
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
  void DelayNetworkCall(base::OnceClosure callback) override;

 private:

  // Helper to delay callbacks until connection becomes online again.
  std::unique_ptr<WaitForNetworkCallbackHelper> network_callback_helper_;
  // The browser state associated with this service.
  ios::ChromeBrowserState* browser_state_;
  // Used to check if sign in cookies are allowed.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // Used to add and remove content settings observers.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeSigninClient);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_IOS_CHROME_SIGNIN_CLIENT_H_
