// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_

#include <set>
#include <string>
#include <vector>

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
  void GetAccessToken(const std::string& gaia_id,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) override;
  std::vector<AccountInfo> GetAllAccounts() const override;
};

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
