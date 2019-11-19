// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_web_state_policy_decider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FakeWebStatePolicyDecider::FakeWebStatePolicyDecider(WebState* web_state)
    : WebStatePolicyDecider(web_state) {}

void FakeWebStatePolicyDecider::SetShouldAllowRequest(
    bool should_allow_request) {
  should_allow_request_ = should_allow_request;
}

bool FakeWebStatePolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    const RequestInfo& request_info) {
  return should_allow_request_;
}

bool FakeWebStatePolicyDecider::ShouldAllowResponse(NSURLResponse* response,
                                                    bool for_main_frame) {
  return true;
}

}  // namespace web