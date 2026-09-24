// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_web_state_policy_decider.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "components/origin_gating/core/origin_gating_configuration.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

namespace {

inline constexpr ActorTaskId kTestTaskId = ActorTaskId(42);

// Fake delegate that returns a fixed decision for testing.
class TestOriginGatingCheckerDelegate
    : public origin_gating::OriginGatingChecker::Delegate {
 public:
  explicit TestOriginGatingCheckerDelegate(bool is_allowed)
      : is_allowed_(is_allowed) {}

  void DoesOriginRequireUserConfirmation(
      origin_gating::GatingDecisionContext* context,
      origin_gating::GateableEvent event,
      const GURL& source,
      const GURL& destination,
      DoesOriginRequireUserConfirmationCallback callback) const override {
    std::move(callback).Run(false);
  }

  void EvaluateEnterprisePolicy(
      const GURL& destination,
      EvaluateEnterprisePolicyCallback callback) const override {
    std::move(callback).Run({.decision = origin_gating::Decision::kNoDecision});
  }

  void OnNoVerdict(
      origin_gating::GatingDecisionContext* context,
      origin_gating::GateableEvent event,
      const GURL& source,
      const GURL& destination,
      bool requires_user_confirmation,
      base::OnceCallback<void(NoVerdictResult)> callback) override {
    std::move(callback).Run({.is_allowed = is_allowed_,
                             .did_prompt_user = false,
                             .bypass_cache = true});
  }

  base::WeakPtr<TestOriginGatingCheckerDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const bool is_allowed_;
  base::WeakPtrFactory<TestOriginGatingCheckerDelegate> weak_ptr_factory_{this};
};
}  // namespace

class ActorWebStatePolicyDeciderTest : public PlatformTest {
 public:
  ActorWebStatePolicyDeciderTest() {
    scoped_feature_list_.InitAndEnableFeature(kActorOriginGating);
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_ = std::make_unique<web::FakeWebState>();
  }

  web::WebStatePolicyDecider::RequestInfo CreateRequestInfo(
      bool target_frame_is_main = true,
      ui::PageTransition transition_type =
          ui::PageTransition::PAGE_TRANSITION_LINK) {
    return web::WebStatePolicyDecider::RequestInfo(
        transition_type,
        /*target_frame_is_main=*/target_frame_is_main,
        /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/false,
        /*user_tapped_recently=*/false);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Test that a permissible navigation request is allowed and do not stop the
// task.
TEST_F(ActorWebStatePolicyDeciderTest, AllowsPermissibleNavigation) {
  TestOriginGatingCheckerDelegate delegate(/*is_allowed=*/true);
  origin_gating::OriginGatingChecker checker(
      delegate.GetWeakPtr(),
      origin_gating::OriginGatingConfiguration(
          /*predicates=*/{}, /*use_site_keyed_cache=*/false));

  base::test::TestFuture<mojom::ActionResultCode> blocked_future;
  ActorWebStatePolicyDecider decider(web_state_.get(), &checker, kTestTaskId,
                                     blocked_future.GetRepeatingCallback());

  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://safe.com"]];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;

  decider.ShouldAllowRequest(request, CreateRequestInfo(),
                             decision_future.GetCallback());

  EXPECT_TRUE(decision_future.Get().ShouldAllowNavigation());
  EXPECT_FALSE(blocked_future.IsReady());
}

// Test that blocked navigations are cancelled and invoke StopTask with
// kBrowserFailure.
TEST_F(ActorWebStatePolicyDeciderTest, CancelsBlockedNavigationAndReportsCode) {
  TestOriginGatingCheckerDelegate delegate(/*is_allowed=*/false);
  origin_gating::OriginGatingChecker checker(
      delegate.GetWeakPtr(),
      origin_gating::OriginGatingConfiguration(
          /*predicates=*/{}, /*use_site_keyed_cache=*/false));

  base::test::TestFuture<mojom::ActionResultCode> blocked_future;
  ActorWebStatePolicyDecider decider(web_state_.get(), &checker, kTestTaskId,
                                     blocked_future.GetRepeatingCallback());

  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://malicious.com"]];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;

  decider.ShouldAllowRequest(request, CreateRequestInfo(),
                             decision_future.GetCallback());

  EXPECT_TRUE(decision_future.Get().ShouldCancelNavigation());
  EXPECT_EQ(blocked_future.Get(),
            mojom::ActionResultCode::kTriggeredNavigationBlocked);
}

// Test that navigation are allowed when the experiment feature flag is
// disabled (default).
TEST_F(ActorWebStatePolicyDeciderTest, AllowsNavigationWhenFeatureDisabled) {
  scoped_feature_list_.Reset();

  // Even if the delegate would block, disabling the feature bypasses the check.
  TestOriginGatingCheckerDelegate delegate(/*is_allowed=*/false);
  origin_gating::OriginGatingChecker checker(
      delegate.GetWeakPtr(),
      origin_gating::OriginGatingConfiguration(
          /*predicates=*/{}, /*use_site_keyed_cache=*/false));

  base::test::TestFuture<mojom::ActionResultCode> blocked_future;
  ActorWebStatePolicyDecider decider(web_state_.get(), &checker, kTestTaskId,
                                     blocked_future.GetRepeatingCallback());

  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://malicious.com"]];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;

  decider.ShouldAllowRequest(request, CreateRequestInfo(),
                             decision_future.GetCallback());

  EXPECT_TRUE(decision_future.Get().ShouldAllowNavigation());
  EXPECT_FALSE(blocked_future.IsReady());
}

// Test that navigations are cancelled when the checker is null.
TEST_F(ActorWebStatePolicyDeciderTest, CancelsNavigationWhenCheckerIsNull) {
  base::test::TestFuture<mojom::ActionResultCode> blocked_future;
  ActorWebStatePolicyDecider decider(web_state_.get(),
                                     /*gating_checker=*/nullptr, kTestTaskId,
                                     blocked_future.GetRepeatingCallback());

  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://malicious.com"]];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;

  decider.ShouldAllowRequest(request, CreateRequestInfo(),
                             decision_future.GetCallback());

  EXPECT_TRUE(decision_future.Get().ShouldCancelNavigation());
  EXPECT_EQ(blocked_future.Get(),
            mojom::ActionResultCode::kTriggeredNavigationBlocked);
}

// Test that subframe navigations are allowed without gating.
TEST_F(ActorWebStatePolicyDeciderTest, AllowsSubframeNavigation) {
  TestOriginGatingCheckerDelegate delegate(/*is_allowed=*/false);
  origin_gating::OriginGatingChecker checker(
      delegate.GetWeakPtr(),
      origin_gating::OriginGatingConfiguration(
          /*predicates=*/{}, /*use_site_keyed_cache=*/false));

  base::test::TestFuture<mojom::ActionResultCode> blocked_future;
  ActorWebStatePolicyDecider decider(web_state_.get(), &checker, kTestTaskId,
                                     blocked_future.GetRepeatingCallback());

  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://malicious.com"]];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;

  decider.ShouldAllowRequest(request,
                             CreateRequestInfo(/*target_frame_is_main*/ false),
                             decision_future.GetCallback());

  EXPECT_TRUE(decision_future.Get().ShouldAllowNavigation());
  EXPECT_FALSE(blocked_future.IsReady());
}

// Test that invalid URLs are rejected.
TEST_F(ActorWebStatePolicyDeciderTest, CancelsInvalidUrl) {
  TestOriginGatingCheckerDelegate delegate(/*is_allowed=*/true);
  origin_gating::OriginGatingChecker checker(
      delegate.GetWeakPtr(),
      origin_gating::OriginGatingConfiguration(
          /*predicates=*/{}, /*use_site_keyed_cache=*/false));

  base::test::TestFuture<mojom::ActionResultCode> blocked_future;
  ActorWebStatePolicyDecider decider(web_state_.get(), &checker, kTestTaskId,
                                     blocked_future.GetRepeatingCallback());

  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:@""]];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;

  decider.ShouldAllowRequest(request, CreateRequestInfo(),
                             decision_future.GetCallback());

  EXPECT_TRUE(decision_future.Get().ShouldCancelNavigation());
  EXPECT_FALSE(blocked_future.IsReady());
}

// Test that initial explicit AUTO_TOPLEVEL navigations bypass origin gating
// even if the destination origin would otherwise be blocked.
TEST_F(ActorWebStatePolicyDeciderTest, AllowsExplicitAutoToplevelNavigation) {
  TestOriginGatingCheckerDelegate delegate(/*is_allowed=*/false);
  origin_gating::OriginGatingChecker checker(
      delegate.GetWeakPtr(),
      origin_gating::OriginGatingConfiguration(
          /*predicates=*/{}, /*use_site_keyed_cache=*/false));

  base::test::TestFuture<mojom::ActionResultCode> blocked_future;
  ActorWebStatePolicyDecider decider(web_state_.get(), &checker, kTestTaskId,
                                     blocked_future.GetRepeatingCallback());

  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://malicious.com"]];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;

  decider.ShouldAllowRequest(
      request,
      CreateRequestInfo(/*target_frame_is_main=*/true,
                        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL),
      decision_future.GetCallback());

  EXPECT_TRUE(decision_future.Get().ShouldAllowNavigation());
  EXPECT_FALSE(blocked_future.IsReady());
}

// Test that redirected AUTO_TOPLEVEL navigations are not bypassed and are
// gated.
TEST_F(ActorWebStatePolicyDeciderTest,
       CancelsRedirectedAutoToplevelNavigation) {
  TestOriginGatingCheckerDelegate delegate(/*is_allowed=*/false);
  origin_gating::OriginGatingChecker checker(
      delegate.GetWeakPtr(),
      origin_gating::OriginGatingConfiguration(/*predicates=*/{},
                                               /*use_site_keyed_cache=*/false));

  base::test::TestFuture<mojom::ActionResultCode> blocked_future;
  ActorWebStatePolicyDecider decider(web_state_.get(), &checker, kTestTaskId,
                                     blocked_future.GetRepeatingCallback());

  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://malicious.com"]];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;

  const ui::PageTransition redirected_transition = ui::PageTransitionFromInt(
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL |
      ui::PageTransition::PAGE_TRANSITION_SERVER_REDIRECT);

  decider.ShouldAllowRequest(
      request,
      CreateRequestInfo(/*target_frame_is_main=*/true, redirected_transition),
      decision_future.GetCallback());

  EXPECT_TRUE(decision_future.Get().ShouldCancelNavigation());
  EXPECT_EQ(blocked_future.Get(),
            mojom::ActionResultCode::kTriggeredNavigationBlocked);
}
}  // namespace actor
