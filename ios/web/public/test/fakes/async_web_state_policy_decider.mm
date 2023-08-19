// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/async_web_state_policy_decider.h"

namespace web {

AsyncWebStatePolicyDecider::AsyncWebStatePolicyDecider(WebState* web_state)
    : WebStatePolicyDecider(web_state) {}

AsyncWebStatePolicyDecider::~AsyncWebStatePolicyDecider() = default;

void AsyncWebStatePolicyDecider::ShouldAllowResponse(
    NSURLResponse* response,
    WebStatePolicyDecider::ResponseInfo response_info,
    WebStatePolicyDecider::PolicyDecisionCallback callback) {
  callback_ = std::move(callback);
}

bool AsyncWebStatePolicyDecider::ReadyToInvokeCallback() const {
  return !callback_.is_null();
}

void AsyncWebStatePolicyDecider::InvokeCallback(
    WebStatePolicyDecider::PolicyDecision decision) {
  std::move(callback_).Run(decision);
}

}  // namespace web
