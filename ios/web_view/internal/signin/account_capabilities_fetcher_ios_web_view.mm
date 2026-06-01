// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/signin/account_capabilities_fetcher_ios_web_view.h"

#import <optional>

namespace ios_web_view {

AccountCapabilitiesFetcherIOSWebView::~AccountCapabilitiesFetcherIOSWebView() =
    default;

AccountCapabilitiesFetcherIOSWebView::AccountCapabilitiesFetcherIOSWebView(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnSomeCapabilitiesFetchedCallback
        on_some_capabilities_fetched_callback,
    AccountCapabilitiesFetcher::OnAllFetchesCompleteCallback
        on_all_fetches_complete_callback)
    : AccountCapabilitiesFetcher(
          account_info,
          fetch_priority,
          std::move(on_some_capabilities_fetched_callback),
          std::move(on_all_fetches_complete_callback)) {}

void AccountCapabilitiesFetcherIOSWebView::StartImpl() {
  CompleteFetchAndMaybeDestroySelf();
}

}  // namespace ios_web_view
