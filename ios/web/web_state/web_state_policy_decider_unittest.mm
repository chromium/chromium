// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/web_state_policy_decider.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using WebStatePolicyDeciderTest = PlatformTest;

// Tests that PolicyDecision::Allow() creates a PolicyDecision with expected
// state.
TEST_F(WebStatePolicyDeciderTest, PolicyDecisionAllow) {
  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());
  EXPECT_FALSE(policy_decision.ShouldCancelNavigation());
  EXPECT_FALSE(policy_decision.ShouldDisplayError());
  EXPECT_NSEQ(nil, policy_decision.GetDisplayError());
}

// Tests that PolicyDecision::Cancel() creates a PolicyDecision with expected
// state.
TEST_F(WebStatePolicyDeciderTest, PolicyDecisionCancel) {
  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Cancel();
  EXPECT_FALSE(policy_decision.ShouldAllowNavigation());
  EXPECT_TRUE(policy_decision.ShouldCancelNavigation());
  EXPECT_FALSE(policy_decision.ShouldDisplayError());
  EXPECT_NSEQ(nil, policy_decision.GetDisplayError());
}

// Tests that PolicyDecision::CancelAndDisplayError() creates a PolicyDecision
// with expected state.
TEST_F(WebStatePolicyDeciderTest, PolicyDecisionCancelAndDisplayError) {
  NSError* error = [NSError errorWithDomain:@"ErrorDomain"
                                       code:123
                                   userInfo:nil];
  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error);
  EXPECT_FALSE(policy_decision.ShouldAllowNavigation());
  EXPECT_TRUE(policy_decision.ShouldCancelNavigation());
  EXPECT_TRUE(policy_decision.ShouldDisplayError());
  EXPECT_NSEQ(error, policy_decision.GetDisplayError());
}
