// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"
#include "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

class GaiaId;

// Implementation of DeviceAccountsProvider.
class DeviceAccountsProviderImpl
    : public DeviceAccountsProvider,
      public ChromeAccountManagerService::Observer {
 public:
  explicit DeviceAccountsProviderImpl(
      ChromeAccountManagerService* account_manager_service);

  DeviceAccountsProviderImpl(const DeviceAccountsProviderImpl&) = delete;
  DeviceAccountsProviderImpl& operator=(const DeviceAccountsProviderImpl&) =
      delete;

  ~DeviceAccountsProviderImpl() override;

  // ios::DeviceAccountsProvider
  void AddObserver(DeviceAccountsProvider::Observer* observer) override;
  void RemoveObserver(DeviceAccountsProvider::Observer* observer) override;

  void GetAccessToken(const GaiaId& gaia_id,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) override;
  std::vector<AccountInfo> GetAccountsForProfile() const override;
  std::vector<AccountInfo> GetAccountsOnDevice() const override;

  // ChromeAccountManagerService::Observer
  void OnIdentitiesOnDeviceChanged() override;
  void OnIdentityOnDeviceUpdated(id<SystemIdentity> identity) override;

 private:
  raw_ptr<ChromeAccountManagerService> account_manager_service_ = nullptr;
  base::ObserverList<DeviceAccountsProvider::Observer, true> observer_list_;
  base::ScopedObservation<ChromeAccountManagerService,
                          DeviceAccountsProviderImpl>
      chrome_account_manager_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
