// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_monitor.h"

#import <optional>
#import <utility>

#import "base/functional/callback.h"
#import "base/notreached.h"
#import "base/run_loop.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/actor/core/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/ios_page_stability_metrics.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/ios_page_stability_monitor_delegate.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock-matchers.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

namespace {

constexpr base::TimeDelta kObservationDelay = base::Milliseconds(100);
constexpr base::TimeDelta kElapsedDuration = base::Milliseconds(50);

// Test delegate that lets tests block on a `base::RunLoop` until
// `PageStabilityMonitor` transitions to a target state.
class TestPageStabilityMonitorDelegate
    : public IOSPageStabilityMonitorDelegate {
 public:
  using IOSPageStabilityMonitorDelegate::IOSPageStabilityMonitorDelegate;
  ~TestPageStabilityMonitorDelegate() override = default;

  void WaitForState(PageStabilityMonitor::State target_state) {
    if (last_state_ == target_state) {
      return;
    }
    base::RunLoop run_loop;
    target_state_ = target_state;
    on_state_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WillMoveToState(
      page_content_annotations::PageStabilityState state) override {
    IOSPageStabilityMonitorDelegate::WillMoveToState(state);
    last_state_ = state;
    if (target_state_ == state && on_state_callback_) {
      target_state_.reset();
      std::move(on_state_callback_).Run();
    }
  }

 private:
  std::optional<PageStabilityMonitor::State> last_state_;
  std::optional<PageStabilityMonitor::State> target_state_;
  base::OnceClosure on_state_callback_;
};

}  // namespace

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

  // Verifies that a non-OK `OnStabilityResult()` other than `kFrameWentAway`
  // preserves `result.code()` in `final_result_` and records
  // `expected_outcome` without stable timing histograms.
  void ExpectNonOkStabilityResultRecordsOutcome(
      ToolExecutionResult result,
      PageStabilityOutcome expected_outcome) {
    base::HistogramTester histogram_tester;
    std::unique_ptr<web::FakeWebFrame> fake_main_frame =
        web::FakeWebFrame::CreateMainWebFrame();
    auto delegate = std::make_unique<TestPageStabilityMonitorDelegate>();
    TestPageStabilityMonitorDelegate* test_delegate = delegate.get();
    PageStabilityMonitor monitor(fake_main_frame->AsWeakPtr(),
                                 std::move(delegate));

    base::test::TestFuture<void> future;
    monitor.NotifyWhenStable(base::TimeDelta(), future.GetCallback());
    test_delegate->WaitForState(PageStabilityMonitor::State::kStartMonitoring);
    ASSERT_EQ(monitor.StateHistoryForTesting().back(),
              PageStabilityMonitor::State::kStartMonitoring);

    TriggerStabilityResult(monitor, result);

    EXPECT_TRUE(future.IsReady());
    EXPECT_EQ(GetFinalResult(monitor).code(), result.code());
    histogram_tester.ExpectUniqueSample(
        kIOSActorPageStabilityOutcomeMetricName,
        static_cast<base::HistogramBase::Sample32>(expected_outcome), 1);
    histogram_tester.ExpectTotalCount(
        kIOSActorPageStabilityTotalTimeToStableMetricName, 0);
  }

  // Verifies that destroying an active `PageStabilityMonitor` in
  // `active_state` while `target_frame_` is alive transitions through
  // `kMojoDisconnected -> kDone`, records `kCancelled`, and drops the callback.
  void ExpectLiveFrameDestructionRecordsCancelled(
      PageStabilityMonitor::State active_state) {
    base::HistogramTester histogram_tester;
    AggregatedJournal journal;
    RecordingJournalObserver observer(journal);

    std::unique_ptr<web::FakeWebFrame> fake_main_frame =
        web::FakeWebFrame::CreateMainWebFrame();
    base::test::TestFuture<void> future;
    auto delegate = std::make_unique<TestPageStabilityMonitorDelegate>(
        TaskId::FromUnsafeValue(123), journal.GetWeakPtr());
    TestPageStabilityMonitorDelegate* test_delegate = delegate.get();
    std::optional<PageStabilityMonitor> monitor;
    monitor.emplace(fake_main_frame->AsWeakPtr(), std::move(delegate));

    switch (active_state) {
      case PageStabilityMonitor::State::kMonitorStartDelay:
        monitor->NotifyWhenStable(kObservationDelay, future.GetCallback());
        break;
      case PageStabilityMonitor::State::kStartMonitoring:
        monitor->NotifyWhenStable(base::TimeDelta(), future.GetCallback());
        test_delegate->WaitForState(
            PageStabilityMonitor::State::kStartMonitoring);
        ASSERT_EQ(monitor->StateHistoryForTesting().back(),
                  PageStabilityMonitor::State::kStartMonitoring);
        break;
      case PageStabilityMonitor::State::kDelayCallback:
        monitor->NotifyWhenStable(base::TimeDelta(), future.GetCallback());
        task_environment_.FastForwardBy(kTestMinWait / 2);
        ASSERT_EQ(monitor->StateHistoryForTesting().back(),
                  PageStabilityMonitor::State::kStartMonitoring);
        TriggerStabilityResult(*monitor, ToolExecutionResult::Ok());
        break;
      case PageStabilityMonitor::State::kInitial:
      case PageStabilityMonitor::State::kWaitForNavigation:
      case PageStabilityMonitor::State::kMonitorCompleted:
      case PageStabilityMonitor::State::kTimeout:
      case PageStabilityMonitor::State::kInvokeCallback:
      case PageStabilityMonitor::State::kRenderFrameGoingAway:
      case PageStabilityMonitor::State::kMojoDisconnected:
      case PageStabilityMonitor::State::kDone:
        NOTREACHED();
    }

    ASSERT_EQ(monitor->StateHistoryForTesting().back(), active_state);
    test_delegate = nullptr;
    monitor.reset();

    EXPECT_FALSE(future.IsReady());
    histogram_tester.ExpectUniqueSample(
        kIOSActorPageStabilityOutcomeMetricName,
        static_cast<base::HistogramBase::Sample32>(
            PageStabilityOutcome::kCancelled),
        1);
    histogram_tester.ExpectTotalCount(
        kIOSActorPageStabilityTotalTimeToStableMetricName, 0);
    histogram_tester.ExpectTotalCount(
        kIOSActorPageStabilityTimeFromMonitoringToStableMetricName, 0);
    histogram_tester.ExpectTotalCount(
        kIOSActorPageStabilityTotalTimeToWebFrameGoingAwayMetricName, 0);

    ASSERT_GE(observer.entries().size(), 4u);
    const size_t entry_count = observer.entries().size();
    EXPECT_EQ(observer.entries()[entry_count - 4]->type,
              mojom::JournalEntryType::kBegin);
    EXPECT_EQ(observer.entries()[entry_count - 4]->event,
              "PageStabilityState: MojoDisconnected");
    EXPECT_EQ(observer.entries()[entry_count - 3]->type,
              mojom::JournalEntryType::kEnd);
    EXPECT_EQ(observer.entries()[entry_count - 3]->event,
              "PageStabilityState: MojoDisconnected");
    EXPECT_EQ(observer.entries()[entry_count - 2]->type,
              mojom::JournalEntryType::kBegin);
    EXPECT_EQ(observer.entries()[entry_count - 2]->event,
              "PageStabilityState: Done");
    EXPECT_EQ(observer.entries()[entry_count - 1]->type,
              mojom::JournalEntryType::kEnd);
    EXPECT_EQ(observer.entries()[entry_count - 1]->event,
              "PageStabilityState: Done");
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
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
  RecordingJournalObserver observer(journal);

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

  // There should be a kInstant event logged inside the track of the pending
  // async entry.
  ASSERT_EQ(observer.entries().size(), 2u);
  EXPECT_EQ(observer.entries()[1]->type, mojom::JournalEntryType::kInstant);
  EXPECT_EQ(observer.entries()[1]->event, "MonitorStartDelay");
  EXPECT_EQ(observer.entries()[1]->track_uuid,
            observer.entries()[0]->track_uuid);

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

// Tests that `OnStabilityResult()` with `kFrameWentAway` records the
// `kWebFrameGoingAway` outcome and timing histogram.
TEST_F(PageStabilityMonitorTest,
       NotifyWhenStable_OnStabilityResultFrameWentAway) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<web::FakeWebFrame> fake_main_frame =
      web::FakeWebFrame::CreateMainWebFrame();
  PageStabilityMonitor monitor(fake_main_frame->AsWeakPtr(),
                               CreateFakeDelegate());

  base::test::TestFuture<void> future;
  monitor.NotifyWhenStable(base::TimeDelta(), future.GetCallback());
  task_environment_.FastForwardBy(kElapsedDuration);
  ASSERT_EQ(monitor.StateHistoryForTesting().back(),
            PageStabilityMonitor::State::kStartMonitoring);

  TriggerStabilityResult(
      monitor, ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(GetFinalResult(monitor).code(),
            mojom::ActionResultCode::kFrameWentAway);
  histogram_tester.ExpectUniqueSample(
      kIOSActorPageStabilityOutcomeMetricName,
      static_cast<base::HistogramBase::Sample32>(
          PageStabilityOutcome::kWebFrameGoingAway),
      1);
  histogram_tester.ExpectUniqueTimeSample(
      kIOSActorPageStabilityTotalTimeToWebFrameGoingAwayMetricName,
      kElapsedDuration, 1);
  histogram_tester.ExpectTotalCount(
      kIOSActorPageStabilityTotalTimeToStableMetricName, 0);
}

// Tests that `OnStabilityResult()` with `kToolTimeout` records `kTimeout` and
// does not record a stable duration sample.
TEST_F(PageStabilityMonitorTest,
       NotifyWhenStable_OnStabilityResultToolTimeout) {
  ExpectNonOkStabilityResultRecordsOutcome(
      ToolExecutionResult(mojom::ActionResultCode::kToolTimeout),
      PageStabilityOutcome::kTimeout);
}

// Tests that `OnStabilityResult()` with a JavaScript dispatch or execution
// error records `kError` while preserving the error code in `final_result_`.
TEST_F(PageStabilityMonitorTest,
       NotifyWhenStable_OnStabilityResultErrorRecordsError) {
  ExpectNonOkStabilityResultRecordsOutcome(
      ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid,
                          InternalToolErrorCode::
                              kJavascriptFeatureFailedToCallJavaScriptFunction),
      PageStabilityOutcome::kError);
}

// Tests that losing `target_frame_` during `kMonitorStartDelay` transitions to
// `kRenderFrameGoingAway` immediately upon entering `kStartMonitoring`.
TEST_F(PageStabilityMonitorTest,
       NotifyWhenStable_FrameDestroyedDuringStartDelay) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<web::FakeWebFrame> fake_main_frame =
      web::FakeWebFrame::CreateMainWebFrame();
  PageStabilityMonitor monitor(fake_main_frame->AsWeakPtr(),
                               CreateFakeDelegate());

  base::test::TestFuture<void> future;
  monitor.NotifyWhenStable(kObservationDelay, future.GetCallback());
  ASSERT_EQ(monitor.StateHistoryForTesting().back(),
            PageStabilityMonitor::State::kMonitorStartDelay);

  fake_main_frame.reset();
  task_environment_.FastForwardBy(kObservationDelay);

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(GetFinalResult(monitor).code(),
            mojom::ActionResultCode::kFrameWentAway);
  histogram_tester.ExpectUniqueSample(
      kIOSActorPageStabilityOutcomeMetricName,
      static_cast<base::HistogramBase::Sample32>(
          PageStabilityOutcome::kWebFrameGoingAway),
      1);
  histogram_tester.ExpectUniqueTimeSample(
      kIOSActorPageStabilityTotalTimeToWebFrameGoingAwayMetricName,
      kObservationDelay, 1);
}

// Tests that destroying an active `PageStabilityMonitor` in any active state
// while `target_frame_` is alive transitions through `kMojoDisconnected`,
// records `kCancelled`, and does not invoke the callback.
TEST_F(PageStabilityMonitorTest,
       Destructor_WhileActiveWithLiveFrame_RecordsCancelled) {
  ExpectLiveFrameDestructionRecordsCancelled(
      PageStabilityMonitor::State::kMonitorStartDelay);
  ExpectLiveFrameDestructionRecordsCancelled(
      PageStabilityMonitor::State::kStartMonitoring);
  ExpectLiveFrameDestructionRecordsCancelled(
      PageStabilityMonitor::State::kDelayCallback);
}

// Tests that destroying an active `PageStabilityMonitor` after `target_frame_`
// is destroyed records the `kWebFrameGoingAway` outcome and timing histogram.
TEST_F(PageStabilityMonitorTest,
       Destructor_WhileActiveWithDestroyedFrame_RecordsWebFrameGoingAway) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<web::FakeWebFrame> fake_main_frame =
      web::FakeWebFrame::CreateMainWebFrame();
  std::optional<PageStabilityMonitor> monitor;
  monitor.emplace(fake_main_frame->AsWeakPtr(), CreateFakeDelegate());

  base::test::TestFuture<void> future;
  monitor->NotifyWhenStable(base::TimeDelta(), future.GetCallback());
  task_environment_.FastForwardBy(kElapsedDuration);
  ASSERT_EQ(monitor->StateHistoryForTesting().back(),
            PageStabilityMonitor::State::kStartMonitoring);

  fake_main_frame.reset();
  monitor.reset();

  EXPECT_TRUE(future.IsReady());
  histogram_tester.ExpectUniqueSample(
      kIOSActorPageStabilityOutcomeMetricName,
      static_cast<base::HistogramBase::Sample32>(
          PageStabilityOutcome::kWebFrameGoingAway),
      1);
  histogram_tester.ExpectUniqueTimeSample(
      kIOSActorPageStabilityTotalTimeToWebFrameGoingAwayMetricName,
      kElapsedDuration, 1);
}

}  // namespace actor
