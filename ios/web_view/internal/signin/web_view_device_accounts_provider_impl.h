// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"

// Implementation of DeviceAccountsProvider.
class WebViewDeviceAccountsProviderImpl : public DeviceAccountsProvider {
 public:
  WebViewDeviceAccountsProviderImpl();

  WebViewDeviceAccountsProviderImpl(const WebViewDeviceAccountsProviderImpl&) =
      delete;
  WebViewDeviceAccountsProviderImpl& operator=(
      const WebViewDeviceAccountsProviderImpl&) = delete;

  ~WebViewDeviceAccountsProviderImpl() override;

  // ios::DeviceAccountsProvider
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetAccessToken(const GaiaId& gaia_id,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) override;
  std::vector<AccountInfo> GetAccountsForProfile() const override;
  std::vector<AccountInfo> GetAccountsOnDevice() const override;

 private:
  base::ObserverList<Observer, true> observer_list_;
};

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
