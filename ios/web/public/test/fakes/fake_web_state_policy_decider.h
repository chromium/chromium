// Copyright 2019 The Chromium Authors. All rights reserved.
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

  // Sets the value returned from |ShouldAllowRequest|.
  void SetShouldAllowRequest(bool should_allow_request);

  // WebStatePolicyDecider overrides
  // Returns the value set with |SetShouldAllowRequest|. Defaults to true.
  bool ShouldAllowRequest(NSURLRequest* request,
                          const RequestInfo& request_info) override;
  // Always returns true to allow |response|.
  bool ShouldAllowResponse(NSURLResponse* response,
                           bool for_main_frame) override;
  void WebStateDestroyed() override {}

 private:
  bool should_allow_request_ = true;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_POLICY_DECIDER_H_
