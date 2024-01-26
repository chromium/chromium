// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#import "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"

class ChromeAccountManagerService;

// Implementation of DeviceAccountsProvider.
class DeviceAccountsProviderImpl : public DeviceAccountsProvider {
 public:
  explicit DeviceAccountsProviderImpl(
      ChromeAccountManagerService* account_manager_service);

  DeviceAccountsProviderImpl(const DeviceAccountsProviderImpl&) = delete;
  DeviceAccountsProviderImpl& operator=(const DeviceAccountsProviderImpl&) =
      delete;

  ~DeviceAccountsProviderImpl() override;

  // ios::DeviceAccountsProvider
  void GetAccessToken(const std::string& gaia_id,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) override;
  std::vector<AccountInfo> GetAllAccounts() const override;

 private:
  raw_ptr<ChromeAccountManagerService> account_manager_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
