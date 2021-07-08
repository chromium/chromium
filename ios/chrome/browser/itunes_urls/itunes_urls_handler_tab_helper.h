// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ITUNES_URLS_ITUNES_URLS_HANDLER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ITUNES_URLS_ITUNES_URLS_HANDLER_TAB_HELPER_H_

#include "base/macros.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

// Enum for the IOS.StoreKit.ITunesURLsHandlingResult UMA histogram to report
// the results of the StoreKit handling.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ITunesUrlsStoreKitHandlingResult {
  // ITunes URL handling failed. This can happen if the Storekit tab helper is
  // null, which in general should not happen, but if it happens StoreKit will
  // not launch and this will value be logged.
  kUrlHandlingFailed = 0,
  // ITunes URL for single app was handled by the StoreKit.
  kSingleAppUrlHandled = 1,
  // ITunes URL for app bundle was handled by the StoreKit.
  kBundleUrlHandled = 2,
  // ITunes URL for app bundle was not handled by the StoreKit.
  kBundleUrlNotHandled = 3,
  kCount
};

// A Tab helper for iTunes Apps URLs handling.
// If a navigation to web page for a supported product in iTunes App Store
// happens while in non off the record browsing mode, this helper will use
// StoreKitTabHelper to present the information of that product. The goal of
// this class is to workaround a bug where appstore website serves the wrong
// content for itunes.apple.com pages, see http://crbug.com/623016.
class ITunesUrlsHandlerTabHelper
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<ITunesUrlsHandlerTabHelper> {
 public:
  ~ITunesUrlsHandlerTabHelper() override;
  explicit ITunesUrlsHandlerTabHelper(web::WebState* web_state);

  // Returns true, if ITunesUrlsHandlerTabHelper can handle the given |url|.
  static bool CanHandleUrl(const GURL& url);

  // web::WebStatePolicyDecider implementation
  web::WebStatePolicyDecider::PolicyDecision ShouldAllowRequest(
      NSURLRequest* request,
      const web::WebStatePolicyDecider::RequestInfo& request_info) override;

 private:
  friend class web::WebStateUserData<ITunesUrlsHandlerTabHelper>;

  // Opens the StoreKit for the given iTunes app |url|.
  void HandleITunesUrl(const GURL& url);

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ITunesUrlsHandlerTabHelper);
};

#endif  // IOS_CHROME_BROWSER_ITUNES_URLS_ITUNES_URLS_HANDLER_TAB_HELPER_H_
