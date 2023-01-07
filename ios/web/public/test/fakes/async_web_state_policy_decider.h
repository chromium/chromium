// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_ASYNC_WEB_STATE_POLICY_DECIDER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_ASYNC_WEB_STATE_POLICY_DECIDER_H_

#import "ios/web/public/navigation/web_state_policy_decider.h"

namespace web {

// A test WebStatePolicyDecider that defers calling the callback passed to
// ShouldAllowResponse rather than calling it synchronously.
class AsyncWebStatePolicyDecider : public WebStatePolicyDecider {
 public:
  explicit AsyncWebStatePolicyDecider(WebState* web_state);

  ~AsyncWebStatePolicyDecider() override;

  // WebStatePolicyDecider override:
  void ShouldAllowResponse(
      NSURLResponse* response,
      WebStatePolicyDecider::ResponseInfo response_info,
      WebStatePolicyDecider::PolicyDecisionCallback callback) override;

  // True if a call to ShouldAllowResponse() has been received but
  // InvokeCallback() has not yet been called.
  bool ReadyToInvokeCallback() const;

  // Runs the callback passed to the last call to ShouldAllowResponse().
  void InvokeCallback(WebStatePolicyDecider::PolicyDecision decision);

 private:
  WebStatePolicyDecider::PolicyDecisionCallback callback_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_ASYNC_WEB_STATE_POLICY_DECIDER_H_
