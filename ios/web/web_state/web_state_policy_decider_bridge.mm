// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebStatePolicyDeciderBridge::WebStatePolicyDeciderBridge(
    web::WebState* web_state,
    id<CRWWebStatePolicyDecider> decider)
    : WebStatePolicyDecider(web_state), decider_(decider) {}

WebStatePolicyDeciderBridge::~WebStatePolicyDeciderBridge() = default;

bool WebStatePolicyDeciderBridge::ShouldAllowRequest(
    NSURLRequest* request,
    const WebStatePolicyDecider::RequestInfo& request_info) {
  if ([decider_
          respondsToSelector:@selector(shouldAllowRequest:requestInfo:)]) {
    return [decider_ shouldAllowRequest:request requestInfo:request_info];
  }
  return true;
}

bool WebStatePolicyDeciderBridge::ShouldAllowResponse(NSURLResponse* response,
                                                      bool for_main_frame) {
  if ([decider_
          respondsToSelector:@selector(shouldAllowResponse:forMainFrame:)]) {
    return [decider_ shouldAllowResponse:response forMainFrame:for_main_frame];
  }
  return true;
}

}  // namespace web
