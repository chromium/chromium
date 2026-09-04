// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_origin_gating_checker_delegate_ios.h"

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "components/origin_gating/core/types.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace actor {
namespace {
using DecisionWithMetadata =
    origin_gating::OriginGatingChecker::Delegate::DecisionWithMetadata;
using NoVerdictResult =
    origin_gating::OriginGatingChecker::Delegate::NoVerdictResult;

constexpr std::string_view kSameOriginUrl1 = "https://example.com/path1";
constexpr std::string_view kSameOriginUrl2 = "https://example.com/path2";
constexpr std::string_view kCrossOriginUrl = "https://other.com/path";

class ActorOriginGatingCheckerDelegateIOSTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;
  ActorOriginGatingCheckerDelegateIOS delegate_;
};

// Tests that navigation requests never require user confirmation, even across
// origins.
TEST_F(
    ActorOriginGatingCheckerDelegateIOSTest,
    DoesOriginRequireUserConfirmation_NavigationRequests_NeverRequiresConfirmation) {
  base::test::TestFuture<bool> future;
  delegate_.DoesOriginRequireUserConfirmation(
      /*context=*/nullptr, origin_gating::GateableEvent::kNavigationRequest,
      GURL(kSameOriginUrl1), GURL(kCrossOriginUrl), future.GetCallback());

  EXPECT_FALSE(future.Get());
}

// Test that same-origin transitions do not require user confirmation.
TEST_F(
    ActorOriginGatingCheckerDelegateIOSTest,
    DoesOriginRequireUserConfirmation_SameOrigin_DoesNotRequireConfirmation) {
  // Navigation responses for same-origin destinations do not require
  // confirmation.
  {
    base::test::TestFuture<bool> future;
    delegate_.DoesOriginRequireUserConfirmation(
        /*context=*/nullptr, origin_gating::GateableEvent::kNavigationResponse,
        GURL(kSameOriginUrl1), GURL(kSameOriginUrl2), future.GetCallback());
    EXPECT_FALSE(future.Get());
  }

  // Page actions for same-origin destinations do not require confirmation.
  {
    base::test::TestFuture<bool> future;
    delegate_.DoesOriginRequireUserConfirmation(
        /*context=*/nullptr, origin_gating::GateableEvent::kPageAction,
        GURL(kSameOriginUrl1), GURL(kSameOriginUrl2), future.GetCallback());
    EXPECT_FALSE(future.Get());
  }
}

// Test that cross-origin transitions require user confirmation for both
// navigation responses and page actions.
TEST_F(ActorOriginGatingCheckerDelegateIOSTest,
       DoesOriginRequireUserConfirmation_CrossOrigin_RequiresConfirmation) {
  // Navigation responses across origins require confirmation.
  {
    base::test::TestFuture<bool> future;
    delegate_.DoesOriginRequireUserConfirmation(
        /*context=*/nullptr, origin_gating::GateableEvent::kNavigationResponse,
        GURL(kSameOriginUrl1), GURL(kCrossOriginUrl), future.GetCallback());
    EXPECT_TRUE(future.Get());
  }

  // Page actions across origins require confirmation.
  {
    base::test::TestFuture<bool> future;
    delegate_.DoesOriginRequireUserConfirmation(
        /*context=*/nullptr, origin_gating::GateableEvent::kPageAction,
        GURL(kSameOriginUrl1), GURL(kCrossOriginUrl), future.GetCallback());
    EXPECT_TRUE(future.Get());
  }
}

// Test that evaluating enterprise policy returns kNoDecision and bypasses
// cache.
TEST_F(ActorOriginGatingCheckerDelegateIOSTest,
       EvaluateEnterprisePolicy_Unmanaged_ReturnsNoDecisionAndBypassesCache) {
  base::test::TestFuture<DecisionWithMetadata> future;
  delegate_.EvaluateEnterprisePolicy(GURL(kSameOriginUrl1),
                                     future.GetCallback());

  const DecisionWithMetadata result = future.Get();
  EXPECT_EQ(result.decision, origin_gating::Decision::kNoDecision);
  EXPECT_TRUE(result.bypass_cache);
}

// Test that OnNoVerdict for navigation requests fails open and bypasses the
// cache.
TEST_F(ActorOriginGatingCheckerDelegateIOSTest,
       OnNoVerdict_ForNavigationRequests_FailsOpenAndBypassesCache) {
  base::test::TestFuture<NoVerdictResult> future;
  delegate_.OnNoVerdict(
      /*context=*/nullptr, origin_gating::GateableEvent::kNavigationRequest,
      GURL(kSameOriginUrl1), GURL(kCrossOriginUrl),
      /*requires_user_confirmation=*/true, future.GetCallback());

  const NoVerdictResult result = future.Get();
  EXPECT_TRUE(result.is_allowed);
  EXPECT_FALSE(result.did_prompt_user);
  EXPECT_TRUE(result.bypass_cache);
}

// Test that OnNoVerdict blocks execution when user confirmation is required.
TEST_F(ActorOriginGatingCheckerDelegateIOSTest,
       OnNoVerdict_ConfirmationRequired_BlocksExecution) {
  base::test::TestFuture<NoVerdictResult> future;
  delegate_.OnNoVerdict(
      /*context=*/nullptr, origin_gating::GateableEvent::kPageAction,
      GURL(kSameOriginUrl1), GURL(kCrossOriginUrl),
      /*requires_user_confirmation=*/true, future.GetCallback());

  const NoVerdictResult result = future.Get();
  EXPECT_FALSE(result.is_allowed);
  EXPECT_FALSE(result.did_prompt_user);
  EXPECT_FALSE(result.bypass_cache);
}

// Test that OnNoVerdict allows execution when user confirmation is not
// required.
TEST_F(ActorOriginGatingCheckerDelegateIOSTest,
       OnNoVerdict_ConfirmationNotRequired_AllowsExecution) {
  base::test::TestFuture<NoVerdictResult> future;
  delegate_.OnNoVerdict(
      /*context=*/nullptr, origin_gating::GateableEvent::kPageAction,
      GURL(kSameOriginUrl1), GURL(kCrossOriginUrl),
      /*requires_user_confirmation=*/false, future.GetCallback());

  const NoVerdictResult result = future.Get();
  EXPECT_TRUE(result.is_allowed);
  EXPECT_FALSE(result.did_prompt_user);
  EXPECT_FALSE(result.bypass_cache);
}

}  // namespace
}  // namespace actor
