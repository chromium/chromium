// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_ACCOUNT_FETCHER_FACTORY_IOS_WEB_VIEW_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_ACCOUNT_FETCHER_FACTORY_IOS_WEB_VIEW_H_

#import "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#import "components/signin/internal/identity_manager/account_fetcher_factory.h"
#import "components/signin/public/identity_manager/account_info.h"

namespace ios_web_view {

// This factory disables capabilities fetching for iOS WebView.
class AccountFetcherFactoryIOSWebView : public AccountFetcherFactory {
 public:
  using OnCompleteCallback = AccountCapabilitiesFetcher::OnCompleteCallback;

  AccountFetcherFactoryIOSWebView();
  ~AccountFetcherFactoryIOSWebView() override;

  AccountFetcherFactoryIOSWebView(const AccountFetcherFactoryIOSWebView&) =
      delete;
  AccountFetcherFactoryIOSWebView& operator=(
      const AccountFetcherFactoryIOSWebView&) = delete;

  // AccountFetcherFactory:
  std::unique_ptr<AccountCapabilitiesFetcher> CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      OnCompleteCallback on_complete_callback) override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_ACCOUNT_FETCHER_FACTORY_IOS_WEB_VIEW_H_
