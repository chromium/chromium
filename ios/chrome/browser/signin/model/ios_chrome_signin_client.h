// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_IOS_CHROME_SIGNIN_CLIENT_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_IOS_CHROME_SIGNIN_CLIENT_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "components/content_settings/core/browser/cookie_settings.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/signin/public/base/signin_client.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "net/cookies/cookie_change_dispatcher.h"

class WaitForNetworkCallbackHelperIOS;

namespace version_info {
enum class Channel;
}

// Concrete implementation of SigninClient for //ios/chrome.
class IOSChromeSigninClient : public SigninClient {
 public:
  IOSChromeSigninClient(
      ProfileIOS* profile,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<HostContentSettingsMap> host_content_settings_map);

  IOSChromeSigninClient(const IOSChromeSigninClient&) = delete;
  IOSChromeSigninClient& operator=(const IOSChromeSigninClient&) = delete;

  ~IOSChromeSigninClient() override;

  // KeyedService implementation.
  void Shutdown() override;

  // SigninClient implementation.
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) override;
  PrefService* GetPrefs() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::CookieManager* GetCookieManager() override;
  network::mojom::NetworkContext* GetNetworkContext() override;
  void DoFinalInit() override;
  bool AreSigninCookiesAllowed() override;
  bool AreSigninCookiesDeletedOnExit() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  bool AreNetworkCallsDelayed() override;
  void DelayNetworkCall(base::OnceClosure callback) override;
  version_info::Channel GetClientChannel() override;
  void OnPrimaryAccountChanged(
      signin::PrimaryAccountChangeEvent event_details) override;

 private:
  // Helper to delay callbacks until connection becomes online again.
  std::unique_ptr<WaitForNetworkCallbackHelperIOS> network_callback_helper_;
  // The profile associated with this service.
  raw_ptr<ProfileIOS> profile_;
  // Used to check if sign in cookies are allowed.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // Used to add and remove content settings observers.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_IOS_CHROME_SIGNIN_CLIENT_H_
