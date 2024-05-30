// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_ACCOUNT_CAPABILITIES_FETCHER_IOS_WEB_VIEW_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_ACCOUNT_CAPABILITIES_FETCHER_IOS_WEB_VIEW_H_

#import "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#import "components/signin/public/identity_manager/account_info.h"

namespace ios_web_view {

// iOS WebView implementation of `AccountCapabilitiesFetcher`.
class AccountCapabilitiesFetcherIOSWebView : public AccountCapabilitiesFetcher {
 public:
  AccountCapabilitiesFetcherIOSWebView(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      OnCompleteCallback on_complete_callback);
  ~AccountCapabilitiesFetcherIOSWebView() override;

  AccountCapabilitiesFetcherIOSWebView(
      const AccountCapabilitiesFetcherIOSWebView&) = delete;
  AccountCapabilitiesFetcherIOSWebView& operator=(
      const AccountCapabilitiesFetcherIOSWebView&) = delete;

 protected:
  // AccountCapabilitiesFetcher:
  void StartImpl() override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_ACCOUNT_CAPABILITIES_FETCHER_IOS_WEB_VIEW_H_
