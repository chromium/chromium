// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"

namespace web {

WebStatePolicyDeciderBridge::WebStatePolicyDeciderBridge(
    web::WebState* web_state,
    id<CRWWebStatePolicyDecider> decider)
    : WebStatePolicyDecider(web_state), decider_(decider) {}

WebStatePolicyDeciderBridge::~WebStatePolicyDeciderBridge() = default;

void WebStatePolicyDeciderBridge::ShouldAllowRequest(
    NSURLRequest* request,
    RequestInfo request_info,
    PolicyDecisionCallback callback) {
  if ([decider_ respondsToSelector:@selector
                (shouldAllowRequest:requestInfo:decisionHandler:)]) {
    __block PolicyDecisionCallback block_callback = std::move(callback);
    [decider_ shouldAllowRequest:request
                     requestInfo:request_info
                 decisionHandler:^(PolicyDecision result) {
                   std::move(block_callback).Run(result);
                 }];
    return;
  }
  std::move(callback).Run(PolicyDecision::Allow());
}

void WebStatePolicyDeciderBridge::ShouldAllowResponse(
    NSURLResponse* response,
    ResponseInfo response_info,
    PolicyDecisionCallback callback) {
  if ([decider_ respondsToSelector:@selector
                (decidePolicyForNavigationResponse:
                                      responseInfo:decisionHandler:)]) {
    __block PolicyDecisionCallback block_callback = std::move(callback);
    [decider_ decidePolicyForNavigationResponse:response
                                   responseInfo:response_info
                                decisionHandler:^(PolicyDecision result) {
                                  std::move(block_callback).Run(result);
                                }];
    return;
  }
  std::move(callback).Run(PolicyDecision::Allow());
}

}  // namespace web
