// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_origin_gating_checker_delegate_ios.h"

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/origin_gating/core/decision.h"
#import "components/origin_gating/core/gateable_event.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_enterprise_policy_checker_ios.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace actor {
namespace {

constexpr int kPolicyEnabled = 0;

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
      /*context=*/nullptr,
      origin_gating::GateableEvent(origin_gating::NavigationRequestEvent{
          .source = GURL(kSameOriginUrl1),
          .destination = GURL(kCrossOriginUrl)}),
      future.GetCallback());

  EXPECT_FALSE(future.Get());
}

// Test that in-page actions never requires user confirmation.
TEST_F(
    ActorOriginGatingCheckerDelegateIOSTest,
    DoesOriginRequireUserConfirmation_PageActions_NeverRequiresConfirmation) {
  base::test::TestFuture<bool> future;
  delegate_.DoesOriginRequireUserConfirmation(
      /*context=*/nullptr,
      origin_gating::GateableEvent(origin_gating::PageActionEvent{
          .destination = GURL(kCrossOriginUrl),
          .tool = origin_gating::ClientTool(ToolType::kClick)}),
      future.GetCallback());

  EXPECT_FALSE(future.Get());
}

// Test that same-origin transitions do not require user confirmation.
TEST_F(
    ActorOriginGatingCheckerDelegateIOSTest,
    DoesOriginRequireUserConfirmation_SameOrigin_DoesNotRequireConfirmation) {
  base::test::TestFuture<bool> future;
  delegate_.DoesOriginRequireUserConfirmation(
      /*context=*/nullptr,
      origin_gating::GateableEvent(origin_gating::NavigationResponseEvent{
          .source = GURL(kSameOriginUrl1),
          .destination = GURL(kSameOriginUrl2)}),
      future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// Test that cross-origin transitions require user confirmation for navigation
// responses.
TEST_F(ActorOriginGatingCheckerDelegateIOSTest,
       DoesOriginRequireUserConfirmation_CrossOrigin_RequiresConfirmation) {
  // Navigation responses across origins require confirmation.
  base::test::TestFuture<bool> future;
  delegate_.DoesOriginRequireUserConfirmation(
      /*context=*/nullptr,
      origin_gating::GateableEvent(origin_gating::NavigationResponseEvent{
          .source = GURL(kSameOriginUrl1),
          .destination = GURL(kCrossOriginUrl)}),
      future.GetCallback());
  EXPECT_TRUE(future.Get());
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

// Test that evaluating enterprise policy with an active policy checker maps
// `kNotBlocked`, `kExplicitlyAllowed`, and `kExplicitlyBlocked` to the
// corresponding `origin_gating::Decision` values and bypasses cache.
TEST_F(ActorOriginGatingCheckerDelegateIOSTest,
       EvaluateEnterprisePolicy_WithPolicyChecker_MapsDecisions) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterIntegerPref(prefs::kGeminiActOnWebSettings,
                                               kPolicyEnabled);
  pref_service.registry()->RegisterListPref(
      prefs::kGeminiActOnWebAllowedForURLs);
  pref_service.registry()->RegisterListPref(
      prefs::kGeminiActOnWebBlockedForURLs);

  pref_service.SetList(prefs::kGeminiActOnWebBlockedForURLs,
                       base::ListValue().Append("blocked.example.com"));
  pref_service.SetList(prefs::kGeminiActOnWebAllowedForURLs,
                       base::ListValue().Append("allowed.example.com"));

  ActorEnterprisePolicyCheckerIOS policy_checker(&pref_service);
  ActorOriginGatingCheckerDelegateIOS delegate_with_policy(&policy_checker);

  // 1. Unlisted URL -> kNoDecision.
  {
    base::test::TestFuture<DecisionWithMetadata> future;
    delegate_with_policy.EvaluateEnterprisePolicy(
        GURL("https://unlisted.example.com"), future.GetCallback());
    const DecisionWithMetadata result = future.Get();
    EXPECT_EQ(result.decision, origin_gating::Decision::kNoDecision);
    EXPECT_TRUE(result.bypass_cache);
  }

  // 2. Allowlisted URL -> kAllowed.
  {
    base::test::TestFuture<DecisionWithMetadata> future;
    delegate_with_policy.EvaluateEnterprisePolicy(
        GURL("https://allowed.example.com"), future.GetCallback());
    const DecisionWithMetadata result = future.Get();
    EXPECT_EQ(result.decision, origin_gating::Decision::kAllowed);
    EXPECT_TRUE(result.bypass_cache);
  }

  // 3. Blocklisted URL -> KBlocked.
  {
    base::test::TestFuture<DecisionWithMetadata> future;
    delegate_with_policy.EvaluateEnterprisePolicy(
        GURL("https://blocked.example.com"), future.GetCallback());
    const DecisionWithMetadata result = future.Get();
    EXPECT_EQ(result.decision, origin_gating::Decision::kBlocked);
    EXPECT_TRUE(result.bypass_cache);
  }
}

// Test that OnNoVerdict for navigation requests fails open and bypasses the
// cache.
TEST_F(ActorOriginGatingCheckerDelegateIOSTest,
       OnNoVerdict_ForNavigationRequests_FailsOpenAndBypassesCache) {
  base::test::TestFuture<NoVerdictResult> future;
  delegate_.OnNoVerdict(
      /*context=*/nullptr,
      origin_gating::GateableEvent(origin_gating::NavigationRequestEvent{
          .source = GURL(kSameOriginUrl1),
          .destination = GURL(kCrossOriginUrl)}),
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
      /*context=*/nullptr,
      origin_gating::GateableEvent(origin_gating::NavigationResponseEvent{
          .source = GURL(kSameOriginUrl1),
          .destination = GURL(kCrossOriginUrl)}),
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
      /*context=*/nullptr,
      origin_gating::GateableEvent(origin_gating::NavigationResponseEvent{
          .source = GURL(kSameOriginUrl1),
          .destination = GURL(kCrossOriginUrl)}),
      /*requires_user_confirmation=*/false, future.GetCallback());

  const NoVerdictResult result = future.Get();
  EXPECT_TRUE(result.is_allowed);
  EXPECT_FALSE(result.did_prompt_user);
  EXPECT_FALSE(result.bypass_cache);
}

}  // namespace
}  // namespace actor
