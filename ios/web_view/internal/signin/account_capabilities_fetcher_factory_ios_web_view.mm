// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/signin/account_capabilities_fetcher_factory_ios_web_view.h"

#import "ios/web_view/internal/signin/account_capabilities_fetcher_ios_web_view.h"

namespace ios_web_view {

AccountCapabilitiesFetcherFactoryIOSWebView::
    AccountCapabilitiesFetcherFactoryIOSWebView() {}

AccountCapabilitiesFetcherFactoryIOSWebView::
    ~AccountCapabilitiesFetcherFactoryIOSWebView() = default;

std::unique_ptr<AccountCapabilitiesFetcher>
AccountCapabilitiesFetcherFactoryIOSWebView::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback) {
  return std::make_unique<AccountCapabilitiesFetcherIOSWebView>(
      account_info, fetch_priority, std::move(on_complete_callback));
}

}  // namespace ios_web_view
