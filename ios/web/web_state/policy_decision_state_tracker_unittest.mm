// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/policy_decision_state_tracker.h"

#import <optional>

#import "base/functional/callback.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace web {

class PolicyDecisionStateTrackerTest : public PlatformTest {
 public:
  PolicyDecisionStateTrackerTest()
      : policy_decision_state_tracker_(base::BindOnce(
            &PolicyDecisionStateTrackerTest::OnDecisionDetermined,
            base::Unretained(this))) {}

  void OnDecisionDetermined(
      WebStatePolicyDecider::PolicyDecision policy_decision) {
    policy_decision_ = policy_decision;
  }

  PolicyDecisionStateTracker policy_decision_state_tracker_;
  std::optional<WebStatePolicyDecider::PolicyDecision> policy_decision_;
};

// Tests the case where every decision received is to allow the navigation, and
// each such decision is received before calling FinishedRequestingDecisions.
TEST_F(PolicyDecisionStateTrackerTest, AllAllowSync) {
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  int num_decisions_requested = 3;
  policy_decision_state_tracker_.FinishedRequestingDecisions(
      num_decisions_requested);

  EXPECT_TRUE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_TRUE(policy_decision_);
  EXPECT_TRUE(policy_decision_->ShouldAllowNavigation());
}

// Tests the case where every decision received is to allow the navigation, and
// each such decision is received after calling FinishedRequestingDecisions.
TEST_F(PolicyDecisionStateTrackerTest, AllAllowAsync) {
  int num_decisions_requested = 3;
  policy_decision_state_tracker_.FinishedRequestingDecisions(
      num_decisions_requested);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_TRUE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_TRUE(policy_decision_);

  EXPECT_TRUE(policy_decision_->ShouldAllowNavigation());
}

// Tests the case where every decision received is to allow the navigation, and
// some decisions are received before calling FinishedRequestingDecisions while
// the rest of the decisions are received later.
TEST_F(PolicyDecisionStateTrackerTest, AllAllowMixed) {
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  int num_decisions_requested = 4;
  policy_decision_state_tracker_.FinishedRequestingDecisions(
      num_decisions_requested);
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_TRUE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_TRUE(policy_decision_);

  EXPECT_TRUE(policy_decision_->ShouldAllowNavigation());
}

// Tests the case where a decision to cancel the navigation is received before
// FinishedRequestingDecisions is called.
TEST_F(PolicyDecisionStateTrackerTest, CancelSync) {
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Cancel());
  EXPECT_TRUE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_TRUE(policy_decision_);

  EXPECT_TRUE(policy_decision_->ShouldCancelNavigation());
  EXPECT_FALSE(policy_decision_->ShouldDisplayError());

  // Verify that additional calls into `policy_decision_state_tracker_` do not
  // lead to additional calls to its callback, which would crash since the
  // callback is a OnceCallback.
  int num_decisions_requested = 4;
  policy_decision_state_tracker_.FinishedRequestingDecisions(
      num_decisions_requested);
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Cancel());
}

// Tests the case where a decision to cancel the navigation is received after
// FinishedRequestingDecisions is called.
TEST_F(PolicyDecisionStateTrackerTest, CancelAsync) {
  NSError* error = [NSError errorWithDomain:@"ErrorDomain" code:1 userInfo:nil];
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error));
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  int num_decisions_requested = 4;
  policy_decision_state_tracker_.FinishedRequestingDecisions(
      num_decisions_requested);
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  // A decision to cancel without an error should take precedence over the
  // decision to show an error.
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Cancel());
  EXPECT_TRUE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_TRUE(policy_decision_);

  EXPECT_TRUE(policy_decision_->ShouldCancelNavigation());
  EXPECT_FALSE(policy_decision_->ShouldDisplayError());

  // Verify that an additional calls into policy_decision_state_tracker_ do not
  // lead to additional calls to its callback, which would crash since the
  // callback is a OnceCallback.
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Cancel());
}

// Tests the case where a decision to show an error is received before
// FinishedRequestingDecisions is called.
TEST_F(PolicyDecisionStateTrackerTest, ShowErrorSync) {
  NSError* error = [NSError errorWithDomain:@"ErrorDomain" code:1 userInfo:nil];
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error));
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  int num_decisions_requested = 3;
  policy_decision_state_tracker_.FinishedRequestingDecisions(
      num_decisions_requested);
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_TRUE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_TRUE(policy_decision_);

  EXPECT_TRUE(policy_decision_->ShouldCancelNavigation());
  EXPECT_TRUE(policy_decision_->ShouldDisplayError());
}

// Tests the case where decisions to show an error are received after
// FinishedRequestingDecisions is called.
TEST_F(PolicyDecisionStateTrackerTest, ShowErrorAsync) {
  int num_decisions_requested = 3;
  policy_decision_state_tracker_.FinishedRequestingDecisions(
      num_decisions_requested);
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  NSError* error1 = [NSError errorWithDomain:@"ErrorDomain"
                                        code:1
                                    userInfo:nil];
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error1));
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  NSError* error2 = [NSError errorWithDomain:@"ErrorDomain"
                                        code:2
                                    userInfo:nil];
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error2));
  EXPECT_TRUE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_TRUE(policy_decision_);

  EXPECT_TRUE(policy_decision_->ShouldCancelNavigation());
  EXPECT_TRUE(policy_decision_->ShouldDisplayError());

  // The error received first should take precedence.
  EXPECT_EQ(policy_decision_->GetDisplayError().code, error1.code);
}

// Tests the case where decisions to show an error are received both before and
// after FinishedRequestingDecisions is called.
TEST_F(PolicyDecisionStateTrackerTest, ShowErrorMixed) {
  NSError* error1 = [NSError errorWithDomain:@"ErrorDomain"
                                        code:1
                                    userInfo:nil];
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error1));
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  int num_decisions_requested = 3;
  policy_decision_state_tracker_.FinishedRequestingDecisions(
      num_decisions_requested);
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  NSError* error2 = [NSError errorWithDomain:@"ErrorDomain"
                                        code:2
                                    userInfo:nil];
  policy_decision_state_tracker_.OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error2));
  EXPECT_TRUE(policy_decision_state_tracker_.DeterminedFinalResult());
  EXPECT_TRUE(policy_decision_);

  EXPECT_TRUE(policy_decision_->ShouldCancelNavigation());
  EXPECT_TRUE(policy_decision_->ShouldDisplayError());

  // The error received first should take precedence.
  EXPECT_EQ(policy_decision_->GetDisplayError().code, error1.code);
}

// Test fixture that supports destroying its PolicyDecisionStateTracker,
// allowing destructor behavior to be tested.
class PolicyDecisionStateTrackerDestructionTest : public PlatformTest {
 public:
  PolicyDecisionStateTrackerDestructionTest()
      : policy_decision_state_tracker_(
            std::make_unique<PolicyDecisionStateTracker>(
                base::BindOnce(&PolicyDecisionStateTrackerDestructionTest::
                                   OnDecisionDetermined,
                               base::Unretained(this)))) {}

  void OnDecisionDetermined(
      WebStatePolicyDecider::PolicyDecision policy_decision) {
    policy_decision_ = policy_decision;
  }

  void DestroyPolicyDecisionStateTracker() {
    policy_decision_state_tracker_.reset();
  }

  std::unique_ptr<PolicyDecisionStateTracker> policy_decision_state_tracker_;
  std::optional<WebStatePolicyDecider::PolicyDecision> policy_decision_;
};

// Tests the case where no decisions have been received by the time the
// PolicyDecisionStateTracker is destroyed.
TEST_F(PolicyDecisionStateTrackerDestructionTest, NoDecisionsReceived) {
  int num_decisions_requested = 3;
  policy_decision_state_tracker_->FinishedRequestingDecisions(
      num_decisions_requested);

  DestroyPolicyDecisionStateTracker();

  EXPECT_TRUE(policy_decision_);
  EXPECT_TRUE(policy_decision_->ShouldCancelNavigation());
}

// Tests the case where some but not all decisions have been received by the
// time the PolicyDecisionStateTracker is destroyed.
TEST_F(PolicyDecisionStateTrackerDestructionTest, OnlySomeDecisionsReceived) {
  int num_decisions_requested = 3;
  policy_decision_state_tracker_->FinishedRequestingDecisions(
      num_decisions_requested);

  policy_decision_state_tracker_->OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_->DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  policy_decision_state_tracker_->OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision::Allow());
  EXPECT_FALSE(policy_decision_state_tracker_->DeterminedFinalResult());
  EXPECT_FALSE(policy_decision_);

  DestroyPolicyDecisionStateTracker();

  EXPECT_TRUE(policy_decision_);
  EXPECT_TRUE(policy_decision_->ShouldCancelNavigation());
}

}  // namespace web
