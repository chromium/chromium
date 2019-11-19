// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
#define IOS_CHROME_BROWSER_SIGNIN_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"

// Implementation of DeviceAccountsProvider.
class DeviceAccountsProviderImpl : public DeviceAccountsProvider {
 public:
  DeviceAccountsProviderImpl();
  ~DeviceAccountsProviderImpl() override;

  // ios::DeviceAccountsProvider
  void GetAccessToken(const std::string& gaia_id,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) override;
  std::vector<AccountInfo> GetAllAccounts() const override;
  AuthenticationErrorCategory GetAuthenticationErrorCategory(
      const std::string& gaia_id,
      NSError* error) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceAccountsProviderImpl);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
