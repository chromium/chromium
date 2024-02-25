// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CAPABILITIES_FETCHER_IOS_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CAPABILITIES_FETCHER_IOS_H_

#import "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

class ChromeAccountManagerService;

namespace ios {

// iOS implementation of `AccountCapabilitiesFetcher`.
class AccountCapabilitiesFetcherIOS : public AccountCapabilitiesFetcher {
 public:
  AccountCapabilitiesFetcherIOS(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      ChromeAccountManagerService* account_manager_service,
      OnCompleteCallback on_complete_callback);
  ~AccountCapabilitiesFetcherIOS() override;

  AccountCapabilitiesFetcherIOS(const AccountCapabilitiesFetcherIOS&) = delete;
  AccountCapabilitiesFetcherIOS& operator=(
      const AccountCapabilitiesFetcherIOS&) = delete;

 protected:
  // AccountCapabilitiesFetcher:
  void StartImpl() override;

 private:
  raw_ptr<ChromeAccountManagerService> account_manager_service_ = nil;
  base::WeakPtrFactory<AccountCapabilitiesFetcherIOS> weak_ptr_factory_{this};
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CAPABILITIES_FETCHER_IOS_H_
