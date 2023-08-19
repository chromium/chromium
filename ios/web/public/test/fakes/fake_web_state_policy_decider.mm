// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_web_state_policy_decider.h"

namespace web {

FakeWebStatePolicyDecider::FakeWebStatePolicyDecider(WebState* web_state)
    : WebStatePolicyDecider(web_state) {}

void FakeWebStatePolicyDecider::SetShouldAllowRequest(
    WebStatePolicyDecider::PolicyDecision should_allow_request) {
  should_allow_request_ = should_allow_request;
}

void FakeWebStatePolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    RequestInfo request_info,
    PolicyDecisionCallback callback) {
  std::move(callback).Run(should_allow_request_);
}

void FakeWebStatePolicyDecider::ShouldAllowResponse(
    NSURLResponse* response,
    ResponseInfo response_info,
    PolicyDecisionCallback callback) {
  std::move(callback).Run(PolicyDecision::Allow());
}

}  // namespace web
