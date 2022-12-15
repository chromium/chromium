// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_ACCOUNT_CAPABILITIES_FETCHER_IOS_H_
#define IOS_CHROME_BROWSER_SIGNIN_ACCOUNT_CAPABILITIES_FETCHER_IOS_H_

#import "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/signin/capabilities_dict.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

namespace ios {

// iOS implementation of `AccountCapabilitiesFetcher`.
class AccountCapabilitiesFetcherIOS : public AccountCapabilitiesFetcher {
 public:
  AccountCapabilitiesFetcherIOS(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback,
      ios::ChromeIdentityService* chrome_identity_service_,
      id<SystemIdentity> system_identity);
  ~AccountCapabilitiesFetcherIOS() override;

  AccountCapabilitiesFetcherIOS(const AccountCapabilitiesFetcherIOS&) = delete;
  AccountCapabilitiesFetcherIOS& operator=(
      const AccountCapabilitiesFetcherIOS&) = delete;

 protected:
  // AccountCapabilitiesFetcher:
  void StartImpl() override;

 private:
  void OnCapabilitiesFetched(CapabilitiesDict* capabilities, NSError* error);

  ios::ChromeIdentityService* chrome_identity_service_ = nullptr;
  __strong id<SystemIdentity> const system_identity_ = nil;
  base::WeakPtrFactory<AccountCapabilitiesFetcherIOS> weak_ptr_factory_{this};
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_ACCOUNT_CAPABILITIES_FETCHER_IOS_H_
