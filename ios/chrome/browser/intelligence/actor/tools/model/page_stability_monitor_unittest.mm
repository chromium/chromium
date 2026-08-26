// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_monitor.h"

#import "base/scoped_observation.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/actor/core/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/ios_page_stability_metrics.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/ios_page_stability_monitor_delegate.h"
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

  std::unique_ptr<IOSPageStabilityMonitorDelegate> CreateFakeDelegate() {
    return std::make_unique<IOSPageStabilityMonitorDelegate>();
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

// Helper class to observe AggregatedJournal entries for testing.
class FakeJournalObserver : public AggregatedJournal::Observer {
 public:
  FakeJournalObserver() = default;
  FakeJournalObserver(const FakeJournalObserver&) = delete;
  FakeJournalObserver& operator=(const FakeJournalObserver&) = delete;

  void WillAddJournalEntry(const AggregatedJournal::Entry& entry) override {
    entries_.push_back(entry.data.Clone());
  }

  const std::vector<mojom::JournalEntryPtr>& entries() const {
    return entries_;
  }

 private:
  std::vector<mojom::JournalEntryPtr> entries_;
};

// Test that notifying when stable returns a frame went away error if the web
// frame is destroyed.
TEST_F(PageStabilityMonitorTest, NotifyWhenStable_FrameWentAway) {
  base::test::TestFuture<void> future;
  base::WeakPtr<web::WebFrame> weak_frame;
  {
    auto fake_main_frame = web::FakeWebFrame::CreateMainWebFrame();
    weak_frame = fake_main_frame->AsWeakPtr();
  }  // fake_main_frame destroyed here.

  PageStabilityMonitor monitor(weak_frame, CreateFakeDelegate());
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

// Test that notifying when stable succeeds after the minimum wait delay when
// there is no observation delay.
TEST_F(PageStabilityMonitorTest, NotifyWhenStable_NoObservationDelay) {
  auto fake_main_frame = web::FakeWebFrame::CreateMainWebFrame();

  PageStabilityMonitor monitor(fake_main_frame->AsWeakPtr(),
                               CreateFakeDelegate());

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
  PageStabilityMonitor monitor(fake_main_frame->AsWeakPtr(),
                               CreateFakeDelegate());

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
  base::HistogramTester histogram_tester;
  auto fake_main_frame = web::FakeWebFrame::CreateMainWebFrame();
  PageStabilityMonitor monitor(fake_main_frame->AsWeakPtr(),
                               CreateFakeDelegate());

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

  histogram_tester.ExpectUniqueSample(
      kIOSActorPageStabilityOutcomeMetricName,
      static_cast<base::HistogramBase::Sample32>(
          PageStabilityOutcome::kTimeout),
      1);

  EXPECT_THAT(
      monitor.StateHistoryForTesting(),
      testing::ElementsAre(PageStabilityMonitor::State::kInitial,
                           PageStabilityMonitor::State::kMonitorStartDelay,
                           PageStabilityMonitor::State::kStartMonitoring,
                           PageStabilityMonitor::State::kTimeout,
                           PageStabilityMonitor::State::kInvokeCallback,
                           PageStabilityMonitor::State::kDone));
}

// Test that the delegate correctly logs events to the AggregatedJournal.
TEST_F(PageStabilityMonitorTest, DelegateLogsToAggregatedJournal) {
  AggregatedJournal journal;
  FakeJournalObserver observer;
  base::ScopedObservation<AggregatedJournal, AggregatedJournal::Observer>
      observation(&observer);
  observation.Observe(&journal);

  TaskId task_id = TaskId::FromUnsafeValue(123);
  auto delegate = std::make_unique<IOSPageStabilityMonitorDelegate>(
      task_id, journal.GetWeakPtr());

  // Simulating events sent by the monitor.
  delegate->WillMoveToState(
      page_content_annotations::PageStabilityState::kStartMonitoring);

  // kBegin event should be logged.
  ASSERT_EQ(observer.entries().size(), 1u);
  EXPECT_EQ(observer.entries()[0]->type, mojom::JournalEntryType::kBegin);
  EXPECT_EQ(observer.entries()[0]->event,
            "PageStabilityState: StartMonitoring");

  // Log instant event.
  delegate->OnEvent(
      page_content_annotations::PageStabilityMonitorStartDelayEvent{
          .delay = base::Seconds(1)});

  // There should be a kInstant event logged inside the track.
  ASSERT_EQ(observer.entries().size(), 2u);
  EXPECT_EQ(observer.entries()[1]->type, mojom::JournalEntryType::kInstant);
  EXPECT_EQ(observer.entries()[1]->event, "MonitorStartDelay");

  // WillMoveToState to another state should log kEnd for StartMonitoring and
  // kBegin for Timeout.
  delegate->WillMoveToState(
      page_content_annotations::PageStabilityState::kTimeout);

  ASSERT_EQ(observer.entries().size(), 4u);
  EXPECT_EQ(observer.entries()[2]->type, mojom::JournalEntryType::kEnd);
  EXPECT_EQ(observer.entries()[2]->event,
            "PageStabilityState: StartMonitoring");
  EXPECT_EQ(observer.entries()[3]->type, mojom::JournalEntryType::kBegin);
  EXPECT_EQ(observer.entries()[3]->event, "PageStabilityState: Timeout");

  // Destructor of delegate should close the active Timeout entry (log kEnd).
  delegate.reset();
  ASSERT_EQ(observer.entries().size(), 5u);
  EXPECT_EQ(observer.entries()[4]->type, mojom::JournalEntryType::kEnd);
  EXPECT_EQ(observer.entries()[4]->event, "PageStabilityState: Timeout");
}

// Test that the delegate logs the stability outcome and timing metrics to UMA.
TEST_F(PageStabilityMonitorTest, LogsToUmaOnSuccess) {
  base::HistogramTester histogram_tester;

  AggregatedJournal journal;
  TaskId task_id = TaskId::FromUnsafeValue(123);
  auto delegate = std::make_unique<IOSPageStabilityMonitorDelegate>(
      task_id, journal.GetWeakPtr());

  // Initialize metrics.
  delegate->OnEvent(page_content_annotations::PageStabilityMonitorStartEvent{});

  // Simulate event sequence sent by the monitor when the page settles before
  // the min-delay.
  delegate->WillMoveToState(
      page_content_annotations::PageStabilityState::kStartMonitoring);
  delegate->WillMoveToState(
      page_content_annotations::PageStabilityState::kMonitorCompleted);
  delegate->WillMoveToState(
      page_content_annotations::PageStabilityState::kDelayCallback);
  delegate->WillMoveToState(
      page_content_annotations::PageStabilityState::kDone);

  // Outcome histogram should contain one sample of kStableBeforeMinDelay.
  histogram_tester.ExpectUniqueSample(
      kIOSActorPageStabilityOutcomeMetricName,
      static_cast<base::HistogramBase::Sample32>(
          PageStabilityOutcome::kStableBeforeMinDelay),
      1);

  histogram_tester.ExpectTotalCount(
      kIOSActorPageStabilityTotalTimeToStableMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kIOSActorPageStabilityTimeFromMonitoringToStableMetricName, 1);
}

}  // namespace actor
