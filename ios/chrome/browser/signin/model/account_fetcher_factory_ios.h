// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_FETCHER_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_FETCHER_FACTORY_IOS_H_

#import "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#import "components/signin/internal/identity_manager/account_fetcher_factory.h"
#import "components/signin/public/identity_manager/account_info.h"

class ChromeAccountManagerService;

namespace ios {

// iOS implementation of `AccountFetcherFactory`.
// This factory creates the link between capabilities cached at the system
// identity manager scope and those available in C++ account information.
class AccountFetcherFactoryIOS : public AccountFetcherFactory {
 public:
  using OnCompleteCallback = AccountCapabilitiesFetcher::OnCompleteCallback;

  explicit AccountFetcherFactoryIOS(
      ChromeAccountManagerService* account_management_service);
  ~AccountFetcherFactoryIOS() override;

  AccountFetcherFactoryIOS(const AccountFetcherFactoryIOS&) = delete;
  AccountFetcherFactoryIOS& operator=(const AccountFetcherFactoryIOS&) = delete;

  // AccountFetcherFactory:
  std::unique_ptr<AccountCapabilitiesFetcher> CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      OnCompleteCallback on_complete_callback) override;

 private:
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_FETCHER_FACTORY_IOS_H_
