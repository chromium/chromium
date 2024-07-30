// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/loader/interactive_detector.h"

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

using PageLoad = ukm::builders::PageLoad;

class NetworkActivityCheckerForTest
    : public InteractiveDetector::NetworkActivityChecker {
 public:
  NetworkActivityCheckerForTest(Document* document)
      : InteractiveDetector::NetworkActivityChecker(document) {}

  virtual void SetActiveConnections(int active_connections) {
    active_connections_ = active_connections;
  }
  int GetActiveConnections() override;

 private:
  int active_connections_ = 0;
};

int NetworkActivityCheckerForTest::GetActiveConnections() {
  return active_connections_;
}

class InteractiveDetectorTest : public testing::Test,
                                public ScopedMockOverlayScrollbars {
 public:
  InteractiveDetectorTest() {
    platform_->AdvanceClockSeconds(1);

    auto test_task_runner = platform_->test_task_runner();
    auto* tick_clock = test_task_runner->GetMockTickClock();
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(
        gfx::Size(), nullptr, nullptr, base::NullCallback(), tick_clock);

    Document* document = &dummy_page_holder_->GetDocument();
    detector_ = MakeGarbageCollected<InteractiveDetector>(
        *document, std::make_unique<NetworkActivityCheckerForTest>(document));
    detector_->SetTaskRunnerForTesting(test_task_runner);
    detector_->SetTickClockForTesting(tick_clock);

    // By this time, the DummyPageHolder has created an InteractiveDetector, and
    // sent DOMContentLoadedEnd. We overwrite it with our new
    // InteractiveDetector, which won't have received any timestamps.
    Supplement<Document>::ProvideTo(*document, detector_.Get());

    // Ensure the document is using the injected InteractiveDetector.
    DCHECK_EQ(detector_, InteractiveDetector::From(*document));
  }

  // Public because it's executed on a task queue.
  void DummyTaskWithDuration(double duration_seconds) {
    platform_->AdvanceClockSeconds(duration_seconds);
    dummy_task_end_time_ = Now();
  }

 protected:
  InteractiveDetector* GetDetector() { return detector_; }

  base::TimeTicks GetDummyTaskEndTime() { return dummy_task_end_time_; }

  NetworkActivityCheckerForTest* GetNetworkActivityChecker() {
    // We know in this test context that network_activity_checker_ is an
    // instance of NetworkActivityCheckerForTest, so this static_cast is safe.
    return static_cast<NetworkActivityCheckerForTest*>(
        detector_->network_activity_checker_.get());
  }

  void SimulateNavigationStart(base::TimeTicks nav_start_time) {
    RunTillTimestamp(nav_start_time);
    detector_->SetNavigationStartTime(nav_start_time);
  }

  void SimulateLongTask(base::TimeTicks start, base::TimeTicks end) {
    CHECK(end - start >= base::Seconds(0.05));
    RunTillTimestamp(end);
    detector_->OnLongTaskDetected(start, end);
  }

  void SimulateDOMContentLoadedEnd(base::TimeTicks dcl_time) {
    RunTillTimestamp(dcl_time);
    detector_->OnDomContentLoadedEnd(dcl_time);
  }

  void SimulateFCPDetected(base::TimeTicks fcp_time,
                           base::TimeTicks detection_time) {
    RunTillTimestamp(detection_time);
    detector_->OnFirstContentfulPaint(fcp_time);
  }

  void SimulateInteractiveInvalidatingInput(base::TimeTicks timestamp) {
    RunTillTimestamp(timestamp);
    detector_->OnInvalidatingInputEvent(timestamp);
  }

  void RunTillTimestamp(base::TimeTicks target_time) {
    base::TimeTicks current_time = Now();
    platform_->RunForPeriod(
        std::max(base::TimeDelta(), target_time - current_time));
  }

  int GetActiveConnections() {
    return GetNetworkActivityChecker()->GetActiveConnections();
  }

  void SetActiveConnections(int active_connections) {
    GetNetworkActivityChecker()->SetActiveConnections(active_connections);
  }

  void SimulateResourceLoadBegin(base::TimeTicks load_begin_time) {
    RunTillTimestamp(load_begin_time);
    detector_->OnResourceLoadBegin(load_begin_time);
    // ActiveConnections is incremented after detector runs OnResourceLoadBegin;
    SetActiveConnections(GetActiveConnections() + 1);
  }

  void SimulateResourceLoadEnd(base::TimeTicks load_finish_time) {
    RunTillTimestamp(load_finish_time);
    int active_connections = GetActiveConnections();
    SetActiveConnections(active_connections - 1);
    detector_->OnResourceLoadEnd(load_finish_time);
  }

  base::TimeTicks Now() { return platform_->test_task_runner()->NowTicks(); }

  base::TimeTicks GetInteractiveTime() { return detector_->interactive_time_; }

  void SetTimeToInteractive(base::TimeTicks interactive_time) {
    detector_->interactive_time_ = interactive_time;
  }

  base::TimeDelta GetTotalBlockingTime() {
    return detector_->ComputeTotalBlockingTime();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return dummy_page_holder_->GetDocument().GetTaskRunner(
        TaskType::kUserInteraction);
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

 private:
  Persistent<InteractiveDetector> detector_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  base::TimeTicks dummy_task_end_time_;
};

// Note: The tests currently assume kTimeToInteractiveWindowSeconds is 5
// seconds. The window size is unlikely to change, and this makes the test
// scenarios significantly easier to write.

// Note: Some of the tests are named W_X_Y_Z, where W, X, Y, Z can any of the
// following events:
// FCP: First Contentful Paint
// DCL: DomContentLoadedEnd
// FcpDetect: Detection of FCP. FCP is a presentation timestamp.
// LT: Long Task
// The name shows the ordering of these events in the test.

TEST_F(InteractiveDetectorTest, FCP_DCL_FcpDetect) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(3));
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(5),
      /* detection_time */ t0 + base::Seconds(7));
  // Run until 5 seconds after FCP.
  RunTillTimestamp((t0 + base::Seconds(5)) + base::Seconds(5.0 + 0.1));
  // Reached TTI at FCP.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(5));
}

TEST_F(InteractiveDetectorTest, DCL_FCP_FcpDetect) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(5));
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(7));
  // Run until 5 seconds after FCP.
  RunTillTimestamp((t0 + base::Seconds(3)) + base::Seconds(5.0 + 0.1));
  // Reached TTI at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(5));
}

TEST_F(InteractiveDetectorTest, InstantDetectionAtFcpDetectIfPossible) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(5));
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(10));
  // Although we just detected FCP, the FCP timestamp is more than
  // kTimeToInteractiveWindowSeconds earlier. We should instantaneously
  // detect that we reached TTI at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(5));
}

TEST_F(InteractiveDetectorTest, FcpDetectFiresAfterLateLongTask) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(3));
  SimulateLongTask(t0 + base::Seconds(9), t0 + base::Seconds(9.1));
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(10));
  // There is a 5 second quiet window after fcp_time - the long task is 6s
  // seconds after fcp_time. We should instantly detect we reached TTI at FCP.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(3));
}

TEST_F(InteractiveDetectorTest, FCP_FcpDetect_DCL) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(5));
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(9));
  // TTI reached at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(9));
}

TEST_F(InteractiveDetectorTest, LongTaskBeforeFCPDoesNotAffectTTI) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(3));
  SimulateLongTask(t0 + base::Seconds(5.1), t0 + base::Seconds(5.2));
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(8),
      /* detection_time */ t0 + base::Seconds(9));
  // Run till 5 seconds after FCP.
  RunTillTimestamp((t0 + base::Seconds(8)) + base::Seconds(5.0 + 0.1));
  // TTI reached at FCP.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(8));
}

TEST_F(InteractiveDetectorTest, DCLDoesNotResetTimer) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(4));
  SimulateLongTask(t0 + base::Seconds(5), t0 + base::Seconds(5.1));
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(8));
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + base::Seconds(5.1)) + base::Seconds(5.0 + 0.1));
  // TTI Reached at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(8));
}

TEST_F(InteractiveDetectorTest, DCL_FCP_FcpDetect_LT) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(3));
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(4),
      /* detection_time */ t0 + base::Seconds(5));
  SimulateLongTask(t0 + base::Seconds(7), t0 + base::Seconds(7.1));
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + base::Seconds(7.1)) + base::Seconds(5.0 + 0.1));
  // TTI reached at long task end.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(7.1));
}

TEST_F(InteractiveDetectorTest, DCL_FCP_LT_FcpDetect) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(3));
  SimulateLongTask(t0 + base::Seconds(7), t0 + base::Seconds(7.1));
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(5));
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + base::Seconds(7.1)) + base::Seconds(5.0 + 0.1));
  // TTI reached at long task end.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(7.1));
}

TEST_F(InteractiveDetectorTest, FCP_FcpDetect_LT_DCL) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(4));
  SimulateLongTask(t0 + base::Seconds(7), t0 + base::Seconds(7.1));
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(8));
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + base::Seconds(7.1)) + base::Seconds(5.0 + 0.1));
  // TTI reached at DCL. Note that we do not need to wait for DCL + 5 seconds.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(8));
}

TEST_F(InteractiveDetectorTest, DclIsMoreThan5sAfterFCP) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(4));
  SimulateLongTask(t0 + base::Seconds(7),
                   t0 + base::Seconds(7.1));  // Long task 1.
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(10));
  // Have not reached TTI yet.
  EXPECT_EQ(GetInteractiveTime(), base::TimeTicks());
  SimulateLongTask(t0 + base::Seconds(11),
                   t0 + base::Seconds(11.1));  // Long task 2.
  // Run till long task 2 end + 5 seconds.
  RunTillTimestamp((t0 + base::Seconds(11.1)) + base::Seconds(5.0 + 0.1));
  // TTI reached at long task 2 end.
  EXPECT_EQ(GetInteractiveTime(), (t0 + base::Seconds(11.1)));
}

TEST_F(InteractiveDetectorTest, NetworkBusyBlocksTTIEvenWhenMainThreadQuiet) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(2));
  SimulateResourceLoadBegin(t0 + base::Seconds(3.4));  // Request 2 start.
  SimulateResourceLoadBegin(
      t0 + base::Seconds(3.5));  // Request 3 start. Network busy.
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(4));
  SimulateLongTask(t0 + base::Seconds(7),
                   t0 + base::Seconds(7.1));          // Long task 1.
  SimulateResourceLoadEnd(t0 + base::Seconds(12.2));  // Network quiet.
  // Network busy kept page from reaching TTI..
  EXPECT_EQ(GetInteractiveTime(), base::TimeTicks());
  SimulateLongTask(t0 + base::Seconds(13),
                   t0 + base::Seconds(13.1));  // Long task 2.
  // Run till 5 seconds after long task 2 end.
  RunTillTimestamp((t0 + base::Seconds(13.1)) + base::Seconds(5.0 + 0.1));
  EXPECT_EQ(GetInteractiveTime(), (t0 + base::Seconds(13.1)));
}

// FCP is a presentation timestamp, which is computed by another process and
// thus received asynchronously by the renderer process. Therefore, there can be
// some delay between the time in which FCP occurs and the time in which FCP is
// detected by the renderer.
TEST_F(InteractiveDetectorTest, LongEnoughQuietWindowBetweenFCPAndFcpDetect) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(2));
  SimulateLongTask(t0 + base::Seconds(2.1),
                   t0 + base::Seconds(2.2));  // Long task 1.
  SimulateLongTask(t0 + base::Seconds(8.2),
                   t0 + base::Seconds(8.3));           // Long task 2.
  SimulateResourceLoadBegin(t0 + base::Seconds(8.4));  // Request 2 start.
  SimulateResourceLoadBegin(
      t0 + base::Seconds(8.5));  // Request 3 start. Network busy.
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(10));
  // Even though network is currently busy and we have long task finishing
  // recently, we should be able to detect that the page already achieved TTI at
  // FCP.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(3));
}

TEST_F(InteractiveDetectorTest, NetworkBusyEndIsNotTTI) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(2));
  SimulateResourceLoadBegin(t0 + base::Seconds(3.4));  // Request 2 start.
  SimulateResourceLoadBegin(
      t0 + base::Seconds(3.5));  // Request 3 start. Network busy.
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(4));
  SimulateLongTask(t0 + base::Seconds(7),
                   t0 + base::Seconds(7.1));  // Long task 1.
  SimulateLongTask(t0 + base::Seconds(13),
                   t0 + base::Seconds(13.1));       // Long task 2.
  SimulateResourceLoadEnd(t0 + base::Seconds(14));  // Network quiet.
  // Run till 5 seconds after network busy end.
  RunTillTimestamp((t0 + base::Seconds(14)) + base::Seconds(5.0 + 0.1));
  // TTI reached at long task 2 end, NOT at network busy end.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(13.1));
}

TEST_F(InteractiveDetectorTest, LateLongTaskWithLateFCPDetection) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(2));
  SimulateResourceLoadBegin(t0 + base::Seconds(3.4));  // Request 2 start.
  SimulateResourceLoadBegin(
      t0 + base::Seconds(3.5));  // Request 3 start. Network busy.
  SimulateLongTask(t0 + base::Seconds(7),
                   t0 + base::Seconds(7.1));       // Long task 1.
  SimulateResourceLoadEnd(t0 + base::Seconds(8));  // Network quiet.
  SimulateLongTask(t0 + base::Seconds(14),
                   t0 + base::Seconds(14.1));  // Long task 2.
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(20));
  // TTI reached at long task 1 end, NOT at long task 2 end.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(7.1));
}

TEST_F(InteractiveDetectorTest, IntermittentNetworkBusyBlocksTTI) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(2));
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(4));
  SimulateLongTask(t0 + base::Seconds(7),
                   t0 + base::Seconds(7.1));           // Long task 1.
  SimulateResourceLoadBegin(t0 + base::Seconds(7.9));  // Active connections: 2
  // Network busy start.
  SimulateResourceLoadBegin(t0 + base::Seconds(8));  // Active connections: 3.
  // Network busy end.
  SimulateResourceLoadEnd(t0 + base::Seconds(8.5));  // Active connections: 2.
  // Network busy start.
  SimulateResourceLoadBegin(t0 + base::Seconds(11));  // Active connections: 3.
  // Network busy end.
  SimulateResourceLoadEnd(t0 + base::Seconds(12));  // Active connections: 2.
  SimulateLongTask(t0 + base::Seconds(14),
                   t0 + base::Seconds(14.1));  // Long task 2.
  // Run till 5 seconds after long task 2 end.
  RunTillTimestamp((t0 + base::Seconds(14.1)) + base::Seconds(5.0 + 0.1));
  // TTI reached at long task 2 end.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(14.1));
}

TEST_F(InteractiveDetectorTest, InvalidatingUserInput) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::Seconds(2));
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Seconds(3),
      /* detection_time */ t0 + base::Seconds(4));
  SimulateInteractiveInvalidatingInput(t0 + base::Seconds(5));
  SimulateLongTask(t0 + base::Seconds(7),
                   t0 + base::Seconds(7.1));  // Long task 1.
  // Run till 5 seconds after long task 2 end.
  RunTillTimestamp((t0 + base::Seconds(7.1)) + base::Seconds(5.0 + 0.1));
  // We still detect interactive time on the blink side even if there is an
  // invalidating user input. Page Load Metrics filters out this value in the
  // browser process for UMA reporting.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::Seconds(7.1));
}

TEST_F(InteractiveDetectorTest, TaskLongerThan5sBlocksTTI) {
  base::TimeTicks t0 = Now();
  GetDetector()->SetNavigationStartTime(t0);

  SimulateDOMContentLoadedEnd(t0 + base::Seconds(2));
  SimulateFCPDetected(t0 + base::Seconds(3), t0 + base::Seconds(4));

  // Post a task with 6 seconds duration.
  GetTaskRunner()->PostTask(
      FROM_HERE, WTF::BindOnce(&InteractiveDetectorTest::DummyTaskWithDuration,
                               WTF::Unretained(this), 6.0));

  platform_->RunUntilIdle();

  // We should be able to detect TTI 5s after the end of long task.
  platform_->RunForPeriodSeconds(5.1);
  EXPECT_EQ(GetInteractiveTime(), GetDummyTaskEndTime());
}

TEST_F(InteractiveDetectorTest, LongTaskAfterTTIDoesNothing) {
  base::TimeTicks t0 = Now();
  GetDetector()->SetNavigationStartTime(t0);

  SimulateDOMContentLoadedEnd(t0 + base::Seconds(2));
  SimulateFCPDetected(t0 + base::Seconds(3), t0 + base::Seconds(4));

  // Long task 1.
  GetTaskRunner()->PostTask(
      FROM_HERE, WTF::BindOnce(&InteractiveDetectorTest::DummyTaskWithDuration,
                               WTF::Unretained(this), 0.1));

  platform_->RunUntilIdle();

  base::TimeTicks long_task_1_end_time = GetDummyTaskEndTime();
  // We should be able to detect TTI 5s after the end of long task.
  platform_->RunForPeriodSeconds(5.1);
  EXPECT_EQ(GetInteractiveTime(), long_task_1_end_time);

  // Long task 2.
  GetTaskRunner()->PostTask(
      FROM_HERE, WTF::BindOnce(&InteractiveDetectorTest::DummyTaskWithDuration,
                               WTF::Unretained(this), 0.1));

  platform_->RunUntilIdle();
  // Wait 5 seconds to see if TTI time changes.
  platform_->RunForPeriodSeconds(5.1);
  // TTI time should not change.
  EXPECT_EQ(GetInteractiveTime(), long_task_1_end_time);
}

// In tests for Total Blocking Time (TBT) we call SetTimeToInteractive() instead
// of allowing TimeToInteractive to occur because the computation is gated
// behind tracing being enabled, which means that they won't run by default. In
// addition, further complication stems from the fact that the vector of
// longtasks is cleared at the end of OnTimeToInteractiveDetected(). Therefore,
// the simplest solution is to manually set all of the relevant variables and
// check the correctness of the method ComputeTotalBlockingTime(). This can be
// revisited if we move TBT computations to occur outside of the trace event.
TEST_F(InteractiveDetectorTest, TotalBlockingTimeZero) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Set a high number of active connections, so that
  // OnTimeToInteractiveDetected() is not called by accident.
  SetActiveConnections(5);
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Milliseconds(100),
      /* detection_time */ t0 + base::Milliseconds(100));

  // Longtask of duration 51ms, but only 50ms occur after FCP.
  SimulateLongTask(t0 + base::Milliseconds(99), t0 + base::Milliseconds(150));
  // Longtask of duration 59ms, but only 49ms occur before TTI.
  SimulateLongTask(t0 + base::Milliseconds(201), t0 + base::Milliseconds(260));
  SetTimeToInteractive(t0 + base::Milliseconds(250));
  EXPECT_EQ(GetTotalBlockingTime(), base::TimeDelta());
}

TEST_F(InteractiveDetectorTest, TotalBlockingTimeNonZero) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Set a high number of active connections, so that
  // OnTimeToInteractiveDetected() is not called by accident.
  SetActiveConnections(5);
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Milliseconds(100),
      /* detection_time */ t0 + base::Milliseconds(100));

  // Longtask fully before FCP.
  SimulateLongTask(t0 + base::Milliseconds(30), t0 + base::Milliseconds(89));
  // Longtask of duration 70ms, 60 ms of which occur after FCP. +10ms to TBT.
  SimulateLongTask(t0 + base::Milliseconds(90), t0 + base::Milliseconds(160));
  // Longtask of duration 80ms between FCP and TTI. +30ms to TBT.
  SimulateLongTask(t0 + base::Milliseconds(200), t0 + base::Milliseconds(280));
  // Longtask of duration 90ms, 70ms of which occur before TTI. +20ms to TBT.
  SimulateLongTask(t0 + base::Milliseconds(300), t0 + base::Milliseconds(390));
  // Longtask fully after TTI.
  SimulateLongTask(t0 + base::Milliseconds(371), t0 + base::Milliseconds(472));
  SetTimeToInteractive(t0 + base::Milliseconds(370));
  EXPECT_EQ(GetTotalBlockingTime(), base::Milliseconds(60));
}

TEST_F(InteractiveDetectorTest, TotalBlockingSingleTask) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Set a high number of active connections, so that
  // OnTimeToInteractiveDetected() is not called by accident.
  SetActiveConnections(5);
  SimulateFCPDetected(
      /* fcp_time */ t0 + base::Milliseconds(100),
      /* detection_time */ t0 + base::Milliseconds(100));

  // Longtask of duration 1s, from navigation start.
  SimulateLongTask(t0, t0 + base::Seconds(1));
  SetTimeToInteractive(t0 + base::Milliseconds(500));
  // Truncated longtask is of length 400. So TBT is 400 - 50 = 350
  EXPECT_EQ(GetTotalBlockingTime(), base::Milliseconds(350));
}

TEST_F(InteractiveDetectorTest, FirstInputDelayForClickOnMobile) {
  auto* detector = GetDetector();
  base::TimeTicks t0 = Now();
  // Pointerdown
  Event* pointerdown = MakeGarbageCollected<Event>(
      event_type_names::kPointerdown, MessageEvent::Bubbles::kYes,
      MessageEvent::Cancelable::kYes, MessageEvent::ComposedMode::kComposed,
      t0);
  pointerdown->SetTrusted(true);
  detector->HandleForInputDelay(*pointerdown, t0, t0 + base::Milliseconds(17));
  EXPECT_FALSE(detector->GetFirstInputDelay().has_value());
  // Pointerup
  Event* pointerup = MakeGarbageCollected<Event>(
      event_type_names::kPointerup, MessageEvent::Bubbles::kYes,
      MessageEvent::Cancelable::kYes, MessageEvent::ComposedMode::kComposed,
      t0 + base::Milliseconds(20));
  pointerup->SetTrusted(true);
  detector->HandleForInputDelay(*pointerup, t0 + base::Milliseconds(20),
                                t0 + base::Milliseconds(50));
  EXPECT_TRUE(detector->GetFirstInputDelay().has_value());
  EXPECT_EQ(detector->GetFirstInputDelay().value(), base::Milliseconds(17));
}

TEST_F(InteractiveDetectorTest,
       FirstInputDelayForClickOnDesktopWithFixEnabled) {
  base::test::ScopedFeatureList feature_list;
  auto* detector = GetDetector();
  base::TimeTicks t0 = Now();
  // Pointerdown
  Event* pointerdown = MakeGarbageCollected<Event>(
      event_type_names::kPointerdown, MessageEvent::Bubbles::kYes,
      MessageEvent::Cancelable::kYes, MessageEvent::ComposedMode::kComposed,
      t0);
  pointerdown->SetTrusted(true);
  detector->HandleForInputDelay(*pointerdown, t0, t0 + base::Milliseconds(17));
  EXPECT_FALSE(detector->GetFirstInputDelay().has_value());
  // Mousedown
  Event* mousedown = MakeGarbageCollected<Event>(
      event_type_names::kMousedown, MessageEvent::Bubbles::kYes,
      MessageEvent::Cancelable::kYes, MessageEvent::ComposedMode::kComposed,
      t0);
  mousedown->SetTrusted(true);
  detector->HandleForInputDelay(*mousedown, t0, t0 + base::Milliseconds(13));
  EXPECT_FALSE(detector->GetFirstInputDelay().has_value());
  // Pointerup
  Event* pointerup = MakeGarbageCollected<Event>(
      event_type_names::kPointerup, MessageEvent::Bubbles::kYes,
      MessageEvent::Cancelable::kYes, MessageEvent::ComposedMode::kComposed,
      t0 + base::Milliseconds(20));
  pointerup->SetTrusted(true);
  detector->HandleForInputDelay(*pointerup, t0 + base::Milliseconds(20),
                                t0 + base::Milliseconds(50));
  EXPECT_TRUE(detector->GetFirstInputDelay().has_value());
  EXPECT_EQ(detector->GetFirstInputDelay().value(), base::Milliseconds(17));
}

}  // namespace blink
