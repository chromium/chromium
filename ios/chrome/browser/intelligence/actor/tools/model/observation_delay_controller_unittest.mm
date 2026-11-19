// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/intelligence/actor/tools/model/observation_delay_controller.h"

#import <memory>
#import <utility>

#import "base/run_loop.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/metrics.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock-matchers.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

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

  void TriggerOnAutofillPredictionsFinished() {
    controller_->OnAutofillPredictionsFinished();
  }

  void TriggerOnPageStable() { controller_->OnPageStable(); }

  void TriggerOnLcpDelayFinished() {
    controller_->OnLcpDelayFinished(ToolExecutionResult::Ok());
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

  std::unique_ptr<web::FakeWebFrame> CreateFakeFrameWithBrowserState(
      bool is_main) {
    if (!js_features_overridden_) {
      web::test::OverrideJavaScriptFeatures(
          &fake_browser_state_,
          {PageStabilityJavaScriptFeature::GetInstance()});
      js_features_overridden_ = true;
    }
    const GURL origin("https://example.com");
    std::unique_ptr<web::FakeWebFrame> frame =
        is_main ? web::FakeWebFrame::CreateMainWebFrame(origin)
                : web::FakeWebFrame::CreateChildWebFrame(origin);
    frame->set_browser_state(&fake_browser_state_);
    return frame;
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  web::FakeBrowserState fake_browser_state_;
  bool js_features_overridden_ = false;
  std::unique_ptr<ObservationDelayController> controller_;
};

TEST_F(ObservationDelayControllerTest, DefaultTimeout) {
  base::test::TestFuture<ObservationDelayController::Result> future;
  web::FakeWebState fake_web_state;
  controller_->Wait(/*web_state=*/fake_web_state.GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());

  // Fast forward past the default timeout to trigger the timeout.
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  // Note: kTimeout is not here since that transition comes after kDone, and we
  // don't support transitioning out of that state.
  EXPECT_THAT(
      controller_->StateHistoryForTesting(),
      testing::ElementsAre(
          ObservationDelayController::State::kInitial,
          ObservationDelayController::State::kWaitForPageStability,
          ObservationDelayController::State::kWaitForLoadCompletion,
          ObservationDelayController::State::kDelayForLcp,
          ObservationDelayController::State::kWaitForAutofillPredictions,
          ObservationDelayController::State::kDone));
}

TEST_F(ObservationDelayControllerTest, ConfiguredTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kActorTools,
      {{"ObservationDelayTimeout", "5s"}, {"PageStabilityEnabled", "true"}});
  base::test::TestFuture<ObservationDelayController::Result> future;
  web::FakeWebState fake_web_state;

  controller_->Wait(/*web_state=*/fake_web_state.GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  // Note: kTimeout is not here since that transition comes after kDone, and we
  // don't support transitioning out of that state.
  EXPECT_THAT(
      controller_->StateHistoryForTesting(),
      testing::ElementsAre(
          ObservationDelayController::State::kInitial,
          ObservationDelayController::State::kWaitForPageStability,
          ObservationDelayController::State::kWaitForLoadCompletion,
          ObservationDelayController::State::kDelayForLcp,
          ObservationDelayController::State::kWaitForAutofillPredictions,
          ObservationDelayController::State::kDone));
}

// Tests that `DidStopLoading()` exits `State::kWaitForLoadCompletion` and
// records only `StateDuration.WaitForLoadCompletion`.
TEST_F(ObservationDelayControllerTest, WaitForLoadCompletion_DidStopLoading) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ObservationDelayController::Result> future;
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetLoading(true);
  controller_->Wait(/*web_state=*/fake_web_state->GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  constexpr base::TimeDelta kLoadDuration = base::Milliseconds(50);
  WaitForState(ObservationDelayController::State::kWaitForLoadCompletion);
  task_environment_.FastForwardBy(kLoadDuration);
  TriggerDidStopLoading(fake_web_state.get());

  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  EXPECT_THAT(
      controller_->StateHistoryForTesting(),
      testing::ElementsAre(
          ObservationDelayController::State::kInitial,
          ObservationDelayController::State::kWaitForPageStability,
          ObservationDelayController::State::kWaitForLoadCompletion,
          ObservationDelayController::State::kDelayForLcp,
          ObservationDelayController::State::kWaitForAutofillPredictions,
          ObservationDelayController::State::kDone));

  histogram_tester.ExpectUniqueTimeSample(
      kIOSActorObservationDelayStateDurationWaitForLoadCompletionMetricName,
      kLoadDuration, 1);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForPageStabilityMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationDelayForLcpMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForAutofillPredictionsMetricName,
      0);
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
  web::FakeWebState fake_web_state;
  fake_web_state.SetLoading(true);
  controller_->Wait(/*web_state=*/fake_web_state.GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  WaitForState(ObservationDelayController::State::kWaitForLoadCompletion);
  TriggerDidStopLoading(&fake_web_state);

  TriggerWebStateDestroyed(&fake_web_state);

  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  ObservationDelayController::State::kDelayForLcp,
                  ObservationDelayController::State::kDone));
}

// Test that WebState destruction during `State::kWaitForLoadCompletion`
// transitions directly to `State::kDone`.
TEST_F(ObservationDelayControllerTest,
       WebStateDestroyed_DuringWaitForLoadCompletion) {
  base::test::TestFuture<ObservationDelayController::Result> future;
  web::FakeWebState fake_web_state;
  fake_web_state.SetLoading(true);
  controller_->Wait(/*web_state=*/fake_web_state.GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  WaitForState(ObservationDelayController::State::kWaitForLoadCompletion);

  TriggerWebStateDestroyed(&fake_web_state);

  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  ObservationDelayController::State::kDone));
}

TEST_F(ObservationDelayControllerTest, DidStartNavigation_UnderMaxNavigations) {
  controller_->SetNavigationCount(19);
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

TEST_F(ObservationDelayControllerTest, DidStartNavigation_MaxNavigations) {
  controller_->SetNavigationCount(20);
  base::test::TestFuture<ObservationDelayController::Result> future;
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetLoading(true);
  controller_->Wait(/*web_state=*/fake_web_state->GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  WaitForState(ObservationDelayController::State::kWaitForLoadCompletion);

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  TriggerDidStartNavigation(fake_web_state.get(), &context);

  // Fast forward past the default timeout to trigger the timeout, because
  // DidStartNavigation was ignored.
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  ObservationDelayController::State::kDidTimeout,
                  ObservationDelayController::State::kDone));
}

// Tests that `ObservationDelayController` delays for LCP when using a main
// frame.
TEST_F(ObservationDelayControllerTest, ShouldDelayForLcp_MainFrame) {
  std::unique_ptr<web::FakeWebFrame> fake_main_frame =
      CreateFakeFrameWithBrowserState(/*is_main=*/true);

  web::FakeWebState fake_web_state;
  base::test::TestFuture<ObservationDelayController::Result> future;
  controller_->Wait(/*web_state=*/fake_web_state.GetWeakPtr(),
                    /*web_frame=*/fake_main_frame->AsWeakPtr(),
                    future.GetCallback());

  // Fast forward past default timeout so ObservationDelayController completes.
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  ObservationDelayController::State::kDelayForLcp,
                  ObservationDelayController::State::kDidTimeout,
                  ObservationDelayController::State::kDone));
}

// Tests that `ObservationDelayController` delays for LCP when using an iframe.
TEST_F(ObservationDelayControllerTest, ShouldDelayForLcp_Iframe) {
  std::unique_ptr<web::FakeWebFrame> fake_child_frame =
      CreateFakeFrameWithBrowserState(/*is_main=*/false);

  web::FakeWebState fake_web_state;
  base::test::TestFuture<ObservationDelayController::Result> future;
  controller_->Wait(/*web_state=*/fake_web_state.GetWeakPtr(),
                    /*web_frame=*/fake_child_frame->AsWeakPtr(),
                    future.GetCallback());

  // Fast forward past default timeout so ObservationDelayController completes.
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  EXPECT_THAT(controller_->StateHistoryForTesting(),
              testing::ElementsAre(
                  ObservationDelayController::State::kInitial,
                  ObservationDelayController::State::kWaitForPageStability,
                  ObservationDelayController::State::kWaitForLoadCompletion,
                  ObservationDelayController::State::kDelayForLcp,
                  ObservationDelayController::State::kDidTimeout,
                  ObservationDelayController::State::kDone));
}

// Tests that `OnAutofillPredictionsFinished()` transitions the controller from
// `State::kWaitForAutofillPredictions` to `State::kDone` and records the state
// duration metric.
TEST_F(ObservationDelayControllerTest, OnAutofillPredictionsFinished_Success) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ObservationDelayController::Result> future;
  web::FakeWebState fake_web_state;

  controller_->Wait(/*web_state=*/fake_web_state.GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  WaitForState(ObservationDelayController::State::kWaitForAutofillPredictions);

  ASSERT_FALSE(future.IsReady());
  TriggerOnAutofillPredictionsFinished();

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
  EXPECT_THAT(
      controller_->StateHistoryForTesting(),
      testing::ElementsAre(
          ObservationDelayController::State::kInitial,
          ObservationDelayController::State::kWaitForPageStability,
          ObservationDelayController::State::kWaitForLoadCompletion,
          ObservationDelayController::State::kDelayForLcp,
          ObservationDelayController::State::kWaitForAutofillPredictions,
          ObservationDelayController::State::kDone));

  histogram_tester.ExpectUniqueTimeSample(
      kIOSActorObservationDelayStateDurationWaitForAutofillPredictionsMetricName,
      base::TimeDelta(), 1);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForPageStabilityMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForLoadCompletionMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationDelayForLcpMetricName, 0);
}

// Tests that `ObservationDelayMetrics` are recorded on successful completion,
// omitting duration metrics when all optional wait states are skipped.
TEST_F(ObservationDelayControllerTest, RecordsMetricsOnSuccess) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ObservationDelayController::Result> future;
  web::FakeWebState fake_web_state;

  controller_->Wait(/*web_state=*/fake_web_state.GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);

  histogram_tester.ExpectUniqueSample(
      kIOSActorObservationDelayDidTimeoutMetricName, false, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSActorObservationDelayDidNavigateMetricName, false, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSActorObservationDelayLcpDelayNeededMetricName, false, 1);

  histogram_tester.ExpectUniqueTimeSample(
      kIOSActorObservationDelayTotalWaitDurationMetricName, base::TimeDelta(),
      1);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForPageStabilityMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForLoadCompletionMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationDelayForLcpMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForAutofillPredictionsMetricName,
      0);
}

// Tests that `ObservationDelayMetrics` records `LcpDelayNeeded=true` and
// `StateDuration.WaitForPageStability` and `StateDuration.DelayForLcp` only
// when those wait states actively run and complete.
TEST_F(ObservationDelayControllerTest,
       RecordsMetrics_ActivePageStabilityAndLcpDelay) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<web::FakeWebFrame> fake_main_frame =
      CreateFakeFrameWithBrowserState(/*is_main=*/true);

  web::FakeWebState fake_web_state;
  base::test::TestFuture<ObservationDelayController::Result> future;
  controller_->Wait(/*web_state=*/fake_web_state.GetWeakPtr(),
                    /*web_frame=*/fake_main_frame->AsWeakPtr(),
                    future.GetCallback());

  constexpr base::TimeDelta kPageStabilityDuration = base::Milliseconds(30);
  constexpr base::TimeDelta kLcpDelayDuration = base::Milliseconds(50);

  WaitForState(ObservationDelayController::State::kWaitForPageStability);
  ASSERT_FALSE(future.IsReady());
  task_environment_.FastForwardBy(kPageStabilityDuration);
  TriggerOnPageStable();

  WaitForState(ObservationDelayController::State::kDelayForLcp);
  ASSERT_FALSE(future.IsReady());
  task_environment_.FastForwardBy(kLcpDelayDuration);
  TriggerOnLcpDelayFinished();

  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);

  histogram_tester.ExpectUniqueSample(
      kIOSActorObservationDelayDidTimeoutMetricName, false, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSActorObservationDelayDidNavigateMetricName, false, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSActorObservationDelayLcpDelayNeededMetricName, true, 1);

  histogram_tester.ExpectUniqueTimeSample(
      kIOSActorObservationDelayTotalWaitDurationMetricName,
      kPageStabilityDuration + kLcpDelayDuration, 1);
  histogram_tester.ExpectUniqueTimeSample(
      kIOSActorObservationDelayStateDurationWaitForPageStabilityMetricName,
      kPageStabilityDuration, 1);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForLoadCompletionMetricName, 0);
  histogram_tester.ExpectUniqueTimeSample(
      kIOSActorObservationDelayStateDurationDelayForLcpMetricName,
      kLcpDelayDuration, 1);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForAutofillPredictionsMetricName,
      0);
}

// Test that ObservationDelayMetrics are recorded properly on timeout.
TEST_F(ObservationDelayControllerTest, RecordsMetricsOnTimeout) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ObservationDelayController::Result> future;
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetLoading(true);

  controller_->Wait(/*web_state=*/fake_web_state->GetWeakPtr(),
                    /*web_frame=*/nullptr, future.GetCallback());
  WaitForState(ObservationDelayController::State::kWaitForLoadCompletion);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);

  histogram_tester.ExpectUniqueSample(
      kIOSActorObservationDelayDidTimeoutMetricName, true, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSActorObservationDelayDidNavigateMetricName, false, 1);

  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayTotalWaitDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForPageStabilityMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForLoadCompletionMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForAutofillPredictionsMetricName,
      0);
}

// Test that ObservationDelayMetrics are recorded properly on navigation.
TEST_F(ObservationDelayControllerTest, RecordsMetricsOnNavigation) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      kIOSActorObservationDelayDidTimeoutMetricName, false, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSActorObservationDelayDidNavigateMetricName, true, 1);

  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayTotalWaitDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForPageStabilityMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForLoadCompletionMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kIOSActorObservationDelayStateDurationWaitForAutofillPredictionsMetricName,
      0);
}

}  // namespace actor
