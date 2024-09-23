// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ITUNES_URLS_MODEL_ITUNES_URLS_HANDLER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ITUNES_URLS_MODEL_ITUNES_URLS_HANDLER_TAB_HELPER_H_

#import "ios/web/public/lazy_web_state_user_data.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"

@protocol WebContentCommands;

// A Tab helper for iTunes Apps URLs handling.
// If a navigation to web page for a supported product in iTunes App Store
// happens while in non off the record browsing mode, this helper will use
// StoreKitTabHelper to present the information of that product. The goal of
// this class is to workaround a bug where appstore website serves the wrong
// content for itunes.apple.com pages, see http://crbug.com/623016.
class ITunesUrlsHandlerTabHelper
    : public web::WebStatePolicyDecider,
      public web::LazyWebStateUserData<ITunesUrlsHandlerTabHelper> {
 public:
  ITunesUrlsHandlerTabHelper(const ITunesUrlsHandlerTabHelper&) = delete;
  ITunesUrlsHandlerTabHelper& operator=(const ITunesUrlsHandlerTabHelper&) =
      delete;

  ~ITunesUrlsHandlerTabHelper() override;
  explicit ITunesUrlsHandlerTabHelper(web::WebState* web_state);

  // Returns true, if ITunesUrlsHandlerTabHelper can handle the given `url`.
  static bool CanHandleUrl(const GURL& url);

  // web::WebStatePolicyDecider implementation
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

  // Sets the command handler for opening content-related UI.
  void SetWebContentsHandler(id<WebContentCommands> handler);

 private:
  friend class web::LazyWebStateUserData<ITunesUrlsHandlerTabHelper>;

  // Opens the StoreKit for the given iTunes app `url`.
  void HandleITunesUrl(const GURL& url);

  __weak id<WebContentCommands> web_content_handler_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_ITUNES_URLS_MODEL_ITUNES_URLS_HANDLER_TAB_HELPER_H_
