// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_monitor.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock-matchers.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class PageStabilityMonitorTest : public PlatformTest {
 protected:
  static constexpr base::TimeDelta kTestTimeout = base::Seconds(2);
  static constexpr base::TimeDelta kTestMinWait = base::Milliseconds(100);

  PageStabilityMonitorTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kActorTools, {{"PageStabilityEnabled", "true"},
                      {"ActorPageStabilityTimeout", "2s"},
                      {"ActorPageStabilityMinWait", "100ms"}});
  }

  void TriggerStabilityResult(PageStabilityMonitor& monitor,
                              ToolExecutionResult result) {
    monitor.OnStabilityResult(result);
  }

  ToolExecutionResult GetFinalResult(const PageStabilityMonitor& monitor) {
    return monitor.final_result_;
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PageStabilityMonitorTest, NotifyWhenStable_FrameWentAway) {
  base::test::TestFuture<void> future;
  base::WeakPtr<web::WebFrame> weak_frame;
  {
    auto fake_main_frame = web::FakeWebFrame::CreateMainWebFrame();
    weak_frame = fake_main_frame->AsWeakPtr();
  }  // fake_main_frame destroyed here.

  PageStabilityMonitor monitor(weak_frame);
  monitor.NotifyWhenStable(base::TimeDelta(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(GetFinalResult(monitor).IsOk());
  EXPECT_EQ(GetFinalResult(monitor).code(),
            mojom::ActionResultCode::kFrameWentAway);

  EXPECT_THAT(
      monitor.StateHistoryForTesting(),
      testing::ElementsAre(PageStabilityMonitor::State::kInitial,
                           PageStabilityMonitor::State::kRenderFrameGoingAway,
                           PageStabilityMonitor::State::kInvokeCallback,
                           PageStabilityMonitor::State::kDone));
}

TEST_F(PageStabilityMonitorTest, NotifyWhenStable_NoObservationDelay) {
  auto fake_main_frame = web::FakeWebFrame::CreateMainWebFrame();

  PageStabilityMonitor monitor(fake_main_frame->AsWeakPtr());

  base::test::TestFuture<void> future;
  monitor.NotifyWhenStable(base::TimeDelta(), future.GetCallback());

  // Return a result before the min wait has passed. The callback should not be
  // invoked yet due to min wait.
  task_environment_.FastForwardBy(kTestMinWait / 2);
  EXPECT_EQ(monitor.StateHistoryForTesting().back(),
            PageStabilityMonitor::State::kStartMonitoring);
  TriggerStabilityResult(monitor, ToolExecutionResult::Ok());
  EXPECT_FALSE(future.IsReady());

  // Fast forward past the min wait delay to trigger the callback
  task_environment_.FastForwardBy(kTestMinWait / 2);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(GetFinalResult(monitor).IsOk());

  EXPECT_THAT(
      monitor.StateHistoryForTesting(),
      testing::ElementsAre(PageStabilityMonitor::State::kInitial,
                           PageStabilityMonitor::State::kMonitorStartDelay,
                           PageStabilityMonitor::State::kStartMonitoring,
                           PageStabilityMonitor::State::kMonitorCompleted,
                           PageStabilityMonitor::State::kDelayCallback,
                           PageStabilityMonitor::State::kInvokeCallback,
                           PageStabilityMonitor::State::kDone));
}

// Ensures we wait the `observation_delay` before starting to monitor the page.
TEST_F(PageStabilityMonitorTest, NotifyWhenStable_ObservesAfterDelay) {
  auto fake_main_frame = web::FakeWebFrame::CreateMainWebFrame();
  PageStabilityMonitor monitor(fake_main_frame->AsWeakPtr());

  base::test::TestFuture<void> future;
  const base::TimeDelta observation_delay = base::Seconds(1);
  monitor.NotifyWhenStable(observation_delay, future.GetCallback());

  // We don't enter kStartMonitoring before the `observation_delay` elapses.
  task_environment_.FastForwardBy(observation_delay - base::Milliseconds(1));
  EXPECT_THAT(
      monitor.StateHistoryForTesting(),
      testing::ElementsAre(PageStabilityMonitor::State::kInitial,
                           PageStabilityMonitor::State::kMonitorStartDelay));

  // Advance past `observation_delay` and the monitoring starts.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(
      monitor.StateHistoryForTesting(),
      testing::ElementsAre(PageStabilityMonitor::State::kInitial,
                           PageStabilityMonitor::State::kMonitorStartDelay,
                           PageStabilityMonitor::State::kStartMonitoring));

  TriggerStabilityResult(monitor, ToolExecutionResult::Ok());
  task_environment_.FastForwardBy(kTestMinWait);
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(GetFinalResult(monitor).IsOk());
  EXPECT_THAT(
      monitor.StateHistoryForTesting(),
      testing::ElementsAre(PageStabilityMonitor::State::kInitial,
                           PageStabilityMonitor::State::kMonitorStartDelay,
                           PageStabilityMonitor::State::kStartMonitoring,
                           PageStabilityMonitor::State::kMonitorCompleted,
                           PageStabilityMonitor::State::kDelayCallback,
                           PageStabilityMonitor::State::kInvokeCallback,
                           PageStabilityMonitor::State::kDone));
}

// Tests that NotifyWhenStable times out based on kActorPageStabilityTimeout.
TEST_F(PageStabilityMonitorTest, NotifyWhenStable_TimesOut) {
  auto fake_main_frame = web::FakeWebFrame::CreateMainWebFrame();
  PageStabilityMonitor monitor(fake_main_frame->AsWeakPtr());

  base::test::TestFuture<void> future;
  monitor.NotifyWhenStable(base::TimeDelta(), future.GetCallback());

  // Fast forward past the timeout.
  task_environment_.FastForwardBy(kTestTimeout - base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());
  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_TRUE(future.IsReady());
  ToolExecutionResult result = GetFinalResult(monitor);
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kToolTimeout);

  EXPECT_THAT(
      monitor.StateHistoryForTesting(),
      testing::ElementsAre(PageStabilityMonitor::State::kInitial,
                           PageStabilityMonitor::State::kMonitorStartDelay,
                           PageStabilityMonitor::State::kStartMonitoring,
                           PageStabilityMonitor::State::kTimeout,
                           PageStabilityMonitor::State::kInvokeCallback,
                           PageStabilityMonitor::State::kDone));
}

}  // namespace actor
