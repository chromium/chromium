// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_FETCHER_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_FETCHER_FACTORY_IOS_H_

#import "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#import "components/signin/internal/identity_manager/account_fetcher_factory.h"
#import "components/signin/public/identity_manager/account_info.h"

class ChromeAccountManagerService;
class ProfileOAuth2TokenService;
class SigninClient;

namespace ios {

// iOS implementation of `AccountFetcherFactory`. Creates platform-specific
// fetchers for account info and capabilities available in the system identity
// manager.
class AccountFetcherFactoryIOS : public AccountFetcherFactory {
 public:
  AccountFetcherFactoryIOS(
      ChromeAccountManagerService* account_management_service,
      ProfileOAuth2TokenService& token_service,
      SigninClient& signin_client);
  ~AccountFetcherFactoryIOS() override;

  AccountFetcherFactoryIOS(const AccountFetcherFactoryIOS&) = delete;
  AccountFetcherFactoryIOS& operator=(const AccountFetcherFactoryIOS&) = delete;

  // AccountFetcherFactory:
  std::unique_ptr<AccountInfoFetcher> CreateAccountInfoFetcher(
      const CoreAccountId& account_id,
      base::OnceCallback<void(std::optional<AccountInfo>)> callback) override;
  std::unique_ptr<AccountCapabilitiesFetcher> CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback)
      override;

 private:
  const raw_ptr<ChromeAccountManagerService> account_manager_service_;
  const raw_ref<ProfileOAuth2TokenService> token_service_;
  const raw_ref<SigninClient> signin_client_;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_FETCHER_FACTORY_IOS_H_
