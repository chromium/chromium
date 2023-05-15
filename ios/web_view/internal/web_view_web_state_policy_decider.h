// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_STATE_POLICY_DECIDER_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_STATE_POLICY_DECIDER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/navigation/web_state_policy_decider.h"

namespace web {
class WebState;
}

@class CWVWebView;

namespace ios_web_view {

// An implementation of web::WebStatePolicyDecider which delegates to:
// DEPRECATED:
//   [web_view.navigationDelegate webView:shouldStartLoadWithRequest:]
//   [web_view.navigationDelegate webView:shouldContinueLoadWithResponse:]
//
// RECOMMENDED:
//   [web_view.navigationDelegate
//   webView:decidePolicyForNavigationAction:decisionHandler:]
//   [web_view.navigationDelegate
//   webView:decidePolicyForNavigationResponse:decisionHandler:]
class WebViewWebStatePolicyDecider : public web::WebStatePolicyDecider {
 public:
  WebViewWebStatePolicyDecider(web::WebState* web_state, CWVWebView* web_view);

  // web::WebStatePolicyDecider overrides:
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
  void ShouldAllowResponse(
      NSURLResponse* response,
      web::WebStatePolicyDecider::ResponseInfo response_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

 private:
  // Delegates to |delegate| property of this web view.
  __weak CWVWebView* web_view_ = nil;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_STATE_POLICY_DECIDER_H_
