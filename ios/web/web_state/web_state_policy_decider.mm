// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/web_state_policy_decider.h"

#import "ios/web/public/web_state.h"
#import "ios/web/web_state/web_state_impl.h"

namespace web {

// static
WebStatePolicyDecider::PolicyDecision
WebStatePolicyDecider::PolicyDecision::Allow() {
  return WebStatePolicyDecider::PolicyDecision(
      WebStatePolicyDecider::PolicyDecision::Decision::kAllow, /*error=*/nil);
}

// static
WebStatePolicyDecider::PolicyDecision
WebStatePolicyDecider::PolicyDecision::Cancel() {
  return WebStatePolicyDecider::PolicyDecision(
      WebStatePolicyDecider::PolicyDecision::Decision::kCancel, /*error=*/nil);
}

// static
WebStatePolicyDecider::PolicyDecision
WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(NSError* error) {
  return WebStatePolicyDecider::PolicyDecision(
      WebStatePolicyDecider::PolicyDecision::Decision::kCancelAndDisplayError,
      error);
}

bool WebStatePolicyDecider::PolicyDecision::ShouldAllowNavigation() const {
  return decision == WebStatePolicyDecider::PolicyDecision::Decision::kAllow;
}

bool WebStatePolicyDecider::PolicyDecision::ShouldCancelNavigation() const {
  return !ShouldAllowNavigation();
}

bool WebStatePolicyDecider::PolicyDecision::ShouldDisplayError() const {
  return decision == WebStatePolicyDecider::PolicyDecision::Decision::
                         kCancelAndDisplayError;
}

NSError* WebStatePolicyDecider::PolicyDecision::GetDisplayError() const {
  return error;
}

WebStatePolicyDecider::WebStatePolicyDecider(WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddPolicyDecider(this);
}

WebStatePolicyDecider::~WebStatePolicyDecider() {
  if (web_state_) {
    web_state_->RemovePolicyDecider(this);
  }
}

void WebStatePolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    RequestInfo request_info,
    PolicyDecisionCallback callback) {
  std::move(callback).Run(PolicyDecision::Allow());
}

void WebStatePolicyDecider::ShouldAllowResponse(
    NSURLResponse* response,
    ResponseInfo response_info,
    PolicyDecisionCallback callback) {
  std::move(callback).Run(PolicyDecision::Allow());
}

void WebStatePolicyDecider::ResetWebState() {
  web_state_->RemovePolicyDecider(this);
  web_state_ = nullptr;
}

}  // namespace web
