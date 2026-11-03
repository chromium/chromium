// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ITUNES_URLS_MODEL_ITUNES_URLS_HANDLER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ITUNES_URLS_MODEL_ITUNES_URLS_HANDLER_TAB_HELPER_H_

#import "base/memory/weak_ptr.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

class GURL;
class OverlayResponse;
@protocol WebContentCommands;

// A Tab helper for iTunes Apps URLs handling.
// If a navigation to web page for a supported product in iTunes App Store
// happens, this helper will use `WebContentCommands` to present the
// information of that product. In off-the-record browsing mode, a confirmation
// dialog is shown before opening the App Store externally. The goal of this
// class is to workaround a bug where appstore website serves the wrong content
// for itunes.apple.com pages, see http://crbug.com/623016.
class ITunesUrlsHandlerTabHelper
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<ITunesUrlsHandlerTabHelper> {
 public:
  ITunesUrlsHandlerTabHelper(const ITunesUrlsHandlerTabHelper&) = delete;
  ITunesUrlsHandlerTabHelper& operator=(const ITunesUrlsHandlerTabHelper&) =
      delete;

  ~ITunesUrlsHandlerTabHelper() override;
  explicit ITunesUrlsHandlerTabHelper(web::WebState* web_state);

  // Returns true, if `ITunesUrlsHandlerTabHelper` can handle the given `url`.
  static bool CanHandleUrl(const GURL& url);

  // web::WebStatePolicyDecider implementation
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
  void ShouldAllowResponse(
      NSURLResponse* response,
      web::WebStatePolicyDecider::ResponseInfo response_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

  // Sets the command handler for opening content-related UI.
  void SetWebContentsHandler(id<WebContentCommands> handler);

 private:
  friend class web::WebStateUserData<ITunesUrlsHandlerTabHelper>;

  // Opens the StoreKit for the given iTunes app `url`.
  void HandleITunesUrl(const GURL& url);

  // Triggers an in-tab prompt to ask the user if the App Store should be opened
  // in off-the-record mode.
  void ShowAppLaunchOverlay(const GURL& url);

  // Called when the confirmation overlay for `url` has completed with
  // `response`.
  void OnShowAppLaunchOverlayDone(const GURL& url, OverlayResponse* response);

  // Called when the attempt to open the App Store externally completes with
  // `success`.
  void OnAppStoreLaunchCompleted(bool success);

  // Called when the failure alert overlay has completed with `response`.
  void OnFailureOverlayDone(OverlayResponse* response);

  // Stores `callback` and `policy_decision` as the pending policy decision,
  // canceling any previously pending callback asynchronously.
  void DelayPolicyDecision(
      web::WebStatePolicyDecider::PolicyDecisionCallback callback,
      web::WebStatePolicyDecider::PolicyDecision policy_decision);

  // Resets `is_prompt_active_` and executes the delayed policy decision
  // callback, if any.
  void CompleteAppLaunchPrompt();

  // Handler for opening content-related UI.
  __weak id<WebContentCommands> web_content_handler_ = nil;

  // Whether there is currently an active confirmation prompt.
  bool is_prompt_active_ = false;

  // Stores the latest policy decision callback and decision that were delayed
  // while a confirmation prompt is active.
  web::WebStatePolicyDecider::PolicyDecisionCallback
      pending_policy_decision_callback_;
  web::WebStatePolicyDecider::PolicyDecision pending_policy_decision_ =
      web::WebStatePolicyDecider::PolicyDecision::Allow();

  // Must be last member to ensure it is destroyed last.
  base::WeakPtrFactory<ITunesUrlsHandlerTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_ITUNES_URLS_MODEL_ITUNES_URLS_HANDLER_TAB_HELPER_H_
