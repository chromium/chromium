// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_fetcher_factory_ios.h"

#import "components/signin/internal/identity_manager/account_info_fetcher_gaia.h"
#import "components/signin/public/base/signin_client.h"
#import "ios/chrome/browser/signin/model/account_capabilities_fetcher_ios.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

namespace ios {

AccountFetcherFactoryIOS::AccountFetcherFactoryIOS(
    ChromeAccountManagerService* account_manager_service,
    ProfileOAuth2TokenService& token_service,
    SigninClient& signin_client)
    : account_manager_service_(account_manager_service),
      token_service_(token_service),
      signin_client_(signin_client) {}

AccountFetcherFactoryIOS::~AccountFetcherFactoryIOS() = default;

std::unique_ptr<AccountInfoFetcher>
AccountFetcherFactoryIOS::CreateAccountInfoFetcher(
    const CoreAccountId& account_id,
    base::OnceCallback<void(std::optional<AccountInfo>)> callback) {
  return std::make_unique<AccountInfoFetcherGaia>(
      *token_service_, signin_client_->GetURLLoaderFactory(), account_id,
      std::move(callback));
}

std::unique_ptr<AccountCapabilitiesFetcher>
AccountFetcherFactoryIOS::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback) {
  return std::make_unique<AccountCapabilitiesFetcherIOS>(
      account_info, fetch_priority, account_manager_service_,
      std::move(on_complete_callback));
}

}  // namespace ios
