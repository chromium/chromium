// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/intelligence/actor/tools/model/observation_delay_controller.h"

#import <memory>
#import <utility>

#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock-matchers.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class ObservationDelayControllerTest : public PlatformTest {
 protected:
  ObservationDelayControllerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeatureWithParameters(
        kActorTools, {{"PageStabilityEnabled", "true"}});
  }

  void SetUp() override {
    PlatformTest::SetUp();
    controller_ = std::make_unique<ObservationDelayController>(
        ActorTaskId{}, /*journal=*/nullptr);
  }

  void TriggerDidStartNavigation(web::WebState* web_state,
                                 web::NavigationContext* context) {
    controller_->DidStartNavigation(web_state, context);
  }

  void TriggerDidStopLoading(web::WebState* web_state) {
    controller_->DidStopLoading(web_state);
  }

  void TriggerWebStateDestroyed(web::WebState* web_state) {
    controller_->WebStateDestroyed(web_state);
  }

  void WaitForState(ObservationDelayController::State target_state) {
    base::RunLoop run_loop;
    controller_->SetStateChangeCallbackForTesting(base::BindRepeating(
        [](base::RepeatingClosure quit_closure,
           ObservationDelayController::State target,
           ObservationDelayController::State current) {
          if (current == target) {
            quit_closure.Run();
          }
        },
        run_loop.QuitClosure(), target_state));
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ObservationDelayController> controller_;
};

TEST_F(ObservationDelayControllerTest, DefaultTimeout) {
  base::test::TestFuture<ObservationDelayController::Result> future;
  controller_->Wait(/*web_state*/ nullptr, /*web_frame=*/nullptr,
                    future.GetCallback());

  // Fast forward past the default timeout to trigger the timeout.
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  // Note: kTimeout is not here since that transition comes after kDone, and we
  // don't support transitioning out of that state.
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  ObservationDelayController::State::kDone));
}

TEST_F(ObservationDelayControllerTest, ConfiguredTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kActorTools,
      {{"ObservationDelayTimeout", "5s"}, {"PageStabilityEnabled", "true"}});
  base::test::TestFuture<ObservationDelayController::Result> future;

  controller_->Wait(/*web_state*/ nullptr, /*web_frame=*/nullptr,
                    future.GetCallback());
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  // Note: kTimeout is not here since that transition comes after kDone, and we
  // don't support transitioning out of that state.
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  ObservationDelayController::State::kDone));
}

TEST_F(ObservationDelayControllerTest, WaitForLoadCompletion_DidStopLoading) {
  base::test::TestFuture<ObservationDelayController::Result> future;
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetLoading(true);
  controller_->Wait(/*web_state=*/fake_web_state->GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  WaitForState(ObservationDelayController::State::kWaitForLoadCompletion);
  TriggerDidStopLoading(fake_web_state.get());

  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  ObservationDelayController::State::kDone));
}

TEST_F(ObservationDelayControllerTest, DidStartNavigation_CrossDocument) {
  base::test::TestFuture<ObservationDelayController::Result> future;
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetLoading(true);
  controller_->Wait(/*web_state=*/fake_web_state->GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  WaitForState(ObservationDelayController::State::kWaitForLoadCompletion);

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  TriggerDidStartNavigation(fake_web_state.get(), &context);

  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kPageNavigated);
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  ObservationDelayController::State::kPageNavigated,
                  ObservationDelayController::State::kDone));
}

TEST_F(ObservationDelayControllerTest, DidStartNavigation_SameDocument) {
  base::test::TestFuture<ObservationDelayController::Result> future;
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetLoading(true);
  controller_->Wait(/*web_state=*/fake_web_state->GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  WaitForState(ObservationDelayController::State::kWaitForLoadCompletion);

  web::FakeNavigationContext context;
  context.SetIsSameDocument(true);
  TriggerDidStartNavigation(fake_web_state.get(), &context);

  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  // There isn't a kPageNavigated state here since we ignore
                  // same document navigations.
                  ObservationDelayController::State::kDidTimeout,
                  ObservationDelayController::State::kDone));
}

TEST_F(ObservationDelayControllerTest, WebStateDestroyed) {
  base::test::TestFuture<ObservationDelayController::Result> future;
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetLoading(true);
  controller_->Wait(/*web_state=*/fake_web_state->GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  WaitForState(ObservationDelayController::State::kWaitForLoadCompletion);

  TriggerWebStateDestroyed(fake_web_state.get());

  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  ObservationDelayController::State::kDone));
}

}  // namespace actor
