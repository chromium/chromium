// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_POLICY_DECIDER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_POLICY_DECIDER_H_

#import "ios/web/public/navigation/web_state_policy_decider.h"

@class NSURLRequest;
@class NSURLResponse;

namespace web {

class WebState;

class FakeWebStatePolicyDecider : public WebStatePolicyDecider {
 public:
  explicit FakeWebStatePolicyDecider(WebState* web_state);
  ~FakeWebStatePolicyDecider() override = default;

  // Sets the value returned from `ShouldAllowRequest`.
  void SetShouldAllowRequest(
      WebStatePolicyDecider::PolicyDecision should_allow_request);

  // WebStatePolicyDecider overrides
  // Always calls `callback` with PolicyDecision::Allow().
  void ShouldAllowRequest(NSURLRequest* request,
                          RequestInfo request_info,
                          PolicyDecisionCallback callback) override;
  // Always calls `callback` with PolicyDecision::Allow().
  void ShouldAllowResponse(NSURLResponse* response,
                           ResponseInfo response_info,
                           PolicyDecisionCallback callback) override;
  void WebStateDestroyed() override {}

 private:
  PolicyDecision should_allow_request_ = PolicyDecision::Allow();
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_POLICY_DECIDER_H_
