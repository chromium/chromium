// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_ACCOUNT_FETCHER_FACTORY_IOS_WEB_VIEW_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_ACCOUNT_FETCHER_FACTORY_IOS_WEB_VIEW_H_

#import "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#import "components/signin/internal/identity_manager/account_fetcher_factory.h"
#import "components/signin/public/identity_manager/account_info.h"

class ProfileOAuth2TokenService;
class SigninClient;

namespace ios_web_view {

// This factory disables capabilities fetching for iOS WebView.
class AccountFetcherFactoryIOSWebView : public AccountFetcherFactory {
 public:
  using OnAllFetchesCompleteCallback =
      AccountCapabilitiesFetcher::OnAllFetchesCompleteCallback;

  AccountFetcherFactoryIOSWebView(ProfileOAuth2TokenService& token_service,
                                  SigninClient& signin_client);
  ~AccountFetcherFactoryIOSWebView() override;

  AccountFetcherFactoryIOSWebView(const AccountFetcherFactoryIOSWebView&) =
      delete;
  AccountFetcherFactoryIOSWebView& operator=(
      const AccountFetcherFactoryIOSWebView&) = delete;

  // AccountFetcherFactory:
  std::unique_ptr<AccountInfoFetcher> CreateAccountInfoFetcher(
      const CoreAccountId& account_id,
      base::OnceCallback<void(std::optional<AccountInfo>)> callback) override;
  std::unique_ptr<AccountCapabilitiesFetcher> CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnSomeCapabilitiesFetchedCallback
          on_some_capabilities_fetched_callback,
      OnAllFetchesCompleteCallback on_all_fetches_complete_callback) override;

 private:
  const raw_ref<ProfileOAuth2TokenService> token_service_;
  const raw_ref<SigninClient> signin_client_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_ACCOUNT_FETCHER_FACTORY_IOS_WEB_VIEW_H_
