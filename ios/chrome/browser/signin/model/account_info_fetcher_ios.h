// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_INFO_FETCHER_IOS_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_INFO_FETCHER_IOS_H_

#import <optional>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "components/signin/internal/identity_manager/account_info_fetcher.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

namespace ios {

// iOS implementation of `AccountInfoFetcher` that retrieves account
// details (such as names and hosted domain) from the local system identity
// manager via `ChromeAccountManagerService`.
class AccountInfoFetcherIos : public AccountInfoFetcher {
 public:
  AccountInfoFetcherIos(
      ChromeAccountManagerService* account_manager_service,
      const GaiaId& gaia_id,
      base::OnceCallback<void(std::optional<AccountInfo>)> callback);

  AccountInfoFetcherIos(const AccountInfoFetcherIos&) = delete;
  AccountInfoFetcherIos& operator=(const AccountInfoFetcherIos&) = delete;

  ~AccountInfoFetcherIos() override;

  void Start() override;

 private:
  void OnHostedDomainFetched(id<SystemIdentity> identity,
                             NSString* hosted_domain,
                             NSError* error);

  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  const GaiaId gaia_id_;
  base::OnceCallback<void(std::optional<AccountInfo>)> callback_;

  base::WeakPtrFactory<AccountInfoFetcherIos> weak_ptr_factory_{this};
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_INFO_FETCHER_IOS_H_
