// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/signin/account_fetcher_factory_ios_web_view.h"

#import "components/signin/internal/identity_manager/account_info_fetcher_gaia.h"
#import "components/signin/public/base/signin_client.h"
#import "ios/web_view/internal/signin/account_capabilities_fetcher_ios_web_view.h"

namespace ios_web_view {

AccountFetcherFactoryIOSWebView::AccountFetcherFactoryIOSWebView(
    ProfileOAuth2TokenService& token_service,
    SigninClient& signin_client)
    : token_service_(token_service), signin_client_(signin_client) {}

AccountFetcherFactoryIOSWebView::~AccountFetcherFactoryIOSWebView() = default;

std::unique_ptr<AccountInfoFetcher>
AccountFetcherFactoryIOSWebView::CreateAccountInfoFetcher(
    const CoreAccountId& account_id,
    base::OnceCallback<void(std::optional<AccountInfo>)> callback) {
  return std::make_unique<AccountInfoFetcherGaia>(
      token_service_.get(), signin_client_->GetURLLoaderFactory(), account_id,
      std::move(callback));
}

std::unique_ptr<AccountCapabilitiesFetcher>
AccountFetcherFactoryIOSWebView::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnSomeCapabilitiesFetchedCallback
        on_some_capabilities_fetched_callback,
    AccountCapabilitiesFetcher::OnAllFetchesCompleteCallback
        on_all_fetches_complete_callback) {
  return std::make_unique<AccountCapabilitiesFetcherIOSWebView>(
      account_info, fetch_priority,
      std::move(on_some_capabilities_fetched_callback),
      std::move(on_all_fetches_complete_callback));
}

}  // namespace ios_web_view
