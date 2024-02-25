// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_IOS_H_

#import "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#import "components/signin/internal/identity_manager/account_capabilities_fetcher_factory.h"
#import "components/signin/public/identity_manager/account_info.h"

class ChromeAccountManagerService;

namespace ios {

// iOS implementation of `AccountCapabilitiesFetcherFactory`.
// This factory creates the link between capabilities cached at the system
// identity manager scope and those available in C++ account information.
class AccountCapabilitiesFetcherFactoryIOS
    : public AccountCapabilitiesFetcherFactory {
 public:
  using OnCompleteCallback = AccountCapabilitiesFetcher::OnCompleteCallback;

  explicit AccountCapabilitiesFetcherFactoryIOS(
      ChromeAccountManagerService* account_management_service);
  ~AccountCapabilitiesFetcherFactoryIOS() override;

  AccountCapabilitiesFetcherFactoryIOS(
      const AccountCapabilitiesFetcherFactoryIOS&) = delete;
  AccountCapabilitiesFetcherFactoryIOS& operator=(
      const AccountCapabilitiesFetcherFactoryIOS&) = delete;

  // AccountCapabilitiesFetcherFactory:
  std::unique_ptr<AccountCapabilitiesFetcher> CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      OnCompleteCallback on_complete_callback) override;

 private:
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_IOS_H_
