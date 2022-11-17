// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_BRIDGE_H_
#define IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/navigation/web_state_policy_decider.h"

typedef void (^PolicyDecisionHandler)(
    web::WebStatePolicyDecider::PolicyDecision);

// Objective-C interface for web::WebStatePolicyDecider.
@protocol CRWWebStatePolicyDecider <NSObject>
@optional

// Invoked by `WebStatePolicyDeciderBridge::ShouldAllowRequest`.
- (void)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:(web::WebStatePolicyDecider::RequestInfo)requestInfo
           decisionHandler:(PolicyDecisionHandler)decisionHandler;

// Invoked by `WebStatePolicyDeciderBridge::ShouldAllowResponse`.
- (void)
    decidePolicyForNavigationResponse:(NSURLResponse*)response
                         responseInfo:(web::WebStatePolicyDecider::ResponseInfo)
                                          responseInfo
                      decisionHandler:(PolicyDecisionHandler)decisionHandler;
@end

namespace web {

// Adapter to use an id<CRWWebStatePolicyDecider> as a
// web::WebStatePolicyDecider.
class WebStatePolicyDeciderBridge : public web::WebStatePolicyDecider {
 public:
  WebStatePolicyDeciderBridge(web::WebState* web_state,
                              id<CRWWebStatePolicyDecider> decider);

  WebStatePolicyDeciderBridge(const WebStatePolicyDeciderBridge&) = delete;
  WebStatePolicyDeciderBridge& operator=(const WebStatePolicyDeciderBridge&) =
      delete;

  ~WebStatePolicyDeciderBridge() override;

  // web::WebStatePolicyDecider methods.
  void ShouldAllowRequest(NSURLRequest* request,
                          RequestInfo request_info,
                          PolicyDecisionCallback callback) override;

  void ShouldAllowResponse(NSURLResponse* response,
                           ResponseInfo response_info,
                           PolicyDecisionCallback callback) override;

 private:
  // CRWWebStatePolicyDecider which receives forwarded calls.
  __weak id<CRWWebStatePolicyDecider> decider_ = nil;
};

}  // web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_BRIDGE_H_
