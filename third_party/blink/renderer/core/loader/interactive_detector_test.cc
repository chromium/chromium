// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/interactive_detector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

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

class InteractiveDetectorTest : public testing::Test {
 public:
  InteractiveDetectorTest() {
    platform_->AdvanceClockSeconds(1);

    auto test_task_runner = platform_->test_task_runner();
    auto* tick_clock = test_task_runner->GetMockTickClock();
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(
        IntSize(), nullptr, nullptr, base::NullCallback(), tick_clock);

    Document* document = &dummy_page_holder_->GetDocument();

    detector_ = MakeGarbageCollected<InteractiveDetector>(
        *document, new NetworkActivityCheckerForTest(document));
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
    CHECK(end - start >= base::TimeDelta::FromSecondsD(0.05));
    RunTillTimestamp(end);
    detector_->OnLongTaskDetected(start, end);
  }

  void SimulateDOMContentLoadedEnd(base::TimeTicks dcl_time) {
    RunTillTimestamp(dcl_time);
    detector_->OnDomContentLoadedEnd(dcl_time);
  }

  void SimulateFMPDetected(base::TimeTicks fmp_time,
                           base::TimeTicks detection_time) {
    RunTillTimestamp(detection_time);
    detector_->OnFirstMeaningfulPaintDetected(
        fmp_time, FirstMeaningfulPaintDetector::kNoUserInput);
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

  base::TimeTicks GetInteractiveTime() {
    return detector_->GetInteractiveTime();
  }

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
// FMP: First Meaningful Paint
// DCL: DomContentLoadedEnd
// FmpDetect: Detection of FMP. FMP is not detected in realtime.
// LT: Long Task
// The name shows the ordering of these events in the test.

TEST_F(InteractiveDetectorTest, FMP_DCL_FmpDetect) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(3));
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(5),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(7));
  // Run until 5 seconds after FMP.
  RunTillTimestamp((t0 + base::TimeDelta::FromSeconds(5)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // Reached TTI at FMP.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSeconds(5));
}

TEST_F(InteractiveDetectorTest, DCL_FMP_FmpDetect) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(5));
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(7));
  // Run until 5 seconds after FMP.
  RunTillTimestamp((t0 + base::TimeDelta::FromSeconds(3)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // Reached TTI at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSeconds(5));
}

TEST_F(InteractiveDetectorTest, InstantDetectionAtFmpDetectIfPossible) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(5));
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(10));
  // Although we just detected FMP, the FMP timestamp is more than
  // kTimeToInteractiveWindowSeconds earlier. We should instantaneously
  // detect that we reached TTI at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSeconds(5));
}

TEST_F(InteractiveDetectorTest, FmpDetectFiresAfterLateLongTask) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(3));
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(9),
                   t0 + base::TimeDelta::FromSecondsD(9.1));
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(10));
  // There is a 5 second quiet window after fmp_time - the long task is 6s
  // seconds after fmp_time. We should instantly detect we reached TTI at FMP.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSeconds(3));
}

TEST_F(InteractiveDetectorTest, FMP_FmpDetect_DCL) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(5));
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(9));
  // TTI reached at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSeconds(9));
}

TEST_F(InteractiveDetectorTest, LongTaskBeforeFMPDoesNotAffectTTI) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(3));
  SimulateLongTask(t0 + base::TimeDelta::FromSecondsD(5.1),
                   t0 + base::TimeDelta::FromSecondsD(5.2));
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(8),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(9));
  // Run till 5 seconds after FMP.
  RunTillTimestamp((t0 + base::TimeDelta::FromSeconds(8)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // TTI reached at FMP.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSeconds(8));
}

TEST_F(InteractiveDetectorTest, DCLDoesNotResetTimer) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(4));
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(5),
                   t0 + base::TimeDelta::FromSecondsD(5.1));
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(8));
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + base::TimeDelta::FromSecondsD(5.1)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // TTI Reached at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSeconds(8));
}

TEST_F(InteractiveDetectorTest, DCL_FMP_FmpDetect_LT) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(3));
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(4),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(5));
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(7),
                   t0 + base::TimeDelta::FromSecondsD(7.1));
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + base::TimeDelta::FromSecondsD(7.1)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // TTI reached at long task end.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSecondsD(7.1));
}

TEST_F(InteractiveDetectorTest, DCL_FMP_LT_FmpDetect) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(3));
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(7),
                   t0 + base::TimeDelta::FromSecondsD(7.1));
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(5));
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + base::TimeDelta::FromSecondsD(7.1)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // TTI reached at long task end.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSecondsD(7.1));
}

TEST_F(InteractiveDetectorTest, FMP_FmpDetect_LT_DCL) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(4));
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(7),
                   t0 + base::TimeDelta::FromSecondsD(7.1));
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(8));
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + base::TimeDelta::FromSecondsD(7.1)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // TTI reached at DCL. Note that we do not need to wait for DCL + 5 seconds.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSeconds(8));
}

TEST_F(InteractiveDetectorTest, DclIsMoreThan5sAfterFMP) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(4));
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(7),
                   t0 + base::TimeDelta::FromSecondsD(7.1));  // Long task 1.
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(10));
  // Have not reached TTI yet.
  EXPECT_EQ(GetInteractiveTime(), base::TimeTicks());
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(11),
                   t0 + base::TimeDelta::FromSecondsD(11.1));  // Long task 2.
  // Run till long task 2 end + 5 seconds.
  RunTillTimestamp((t0 + base::TimeDelta::FromSecondsD(11.1)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // TTI reached at long task 2 end.
  EXPECT_EQ(GetInteractiveTime(), (t0 + base::TimeDelta::FromSecondsD(11.1)));
}

TEST_F(InteractiveDetectorTest, NetworkBusyBlocksTTIEvenWhenMainThreadQuiet) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(2));
  SimulateResourceLoadBegin(
      t0 + base::TimeDelta::FromSecondsD(3.4));  // Request 2 start.
  SimulateResourceLoadBegin(t0 + base::TimeDelta::FromSecondsD(
                                     3.5));  // Request 3 start. Network busy.
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(4));
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(7),
                   t0 + base::TimeDelta::FromSecondsD(7.1));  // Long task 1.
  SimulateResourceLoadEnd(
      t0 + base::TimeDelta::FromSecondsD(12.2));  // Network quiet.
  // Network busy kept page from reaching TTI..
  EXPECT_EQ(GetInteractiveTime(), base::TimeTicks());
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(13),
                   t0 + base::TimeDelta::FromSecondsD(13.1));  // Long task 2.
  // Run till 5 seconds after long task 2 end.
  RunTillTimestamp((t0 + base::TimeDelta::FromSecondsD(13.1)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  EXPECT_EQ(GetInteractiveTime(), (t0 + base::TimeDelta::FromSecondsD(13.1)));
}

TEST_F(InteractiveDetectorTest, LongEnoughQuietWindowBetweenFMPAndFmpDetect) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(2));
  SimulateLongTask(t0 + base::TimeDelta::FromSecondsD(2.1),
                   t0 + base::TimeDelta::FromSecondsD(2.2));  // Long task 1.
  SimulateLongTask(t0 + base::TimeDelta::FromSecondsD(8.2),
                   t0 + base::TimeDelta::FromSecondsD(8.3));  // Long task 2.
  SimulateResourceLoadBegin(
      t0 + base::TimeDelta::FromSecondsD(8.4));  // Request 2 start.
  SimulateResourceLoadBegin(t0 + base::TimeDelta::FromSecondsD(
                                     8.5));  // Request 3 start. Network busy.
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(10));
  // Even though network is currently busy and we have long task finishing
  // recently, we should be able to detect that the page already achieved TTI at
  // FMP.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSeconds(3));
}

TEST_F(InteractiveDetectorTest, NetworkBusyEndIsNotTTI) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(2));
  SimulateResourceLoadBegin(
      t0 + base::TimeDelta::FromSecondsD(3.4));  // Request 2 start.
  SimulateResourceLoadBegin(t0 + base::TimeDelta::FromSecondsD(
                                     3.5));  // Request 3 start. Network busy.
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(4));
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(7),
                   t0 + base::TimeDelta::FromSecondsD(7.1));  // Long task 1.
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(13),
                   t0 + base::TimeDelta::FromSecondsD(13.1));  // Long task 2.
  SimulateResourceLoadEnd(t0 +
                          base::TimeDelta::FromSeconds(14));  // Network quiet.
  // Run till 5 seconds after network busy end.
  RunTillTimestamp((t0 + base::TimeDelta::FromSeconds(14)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // TTI reached at long task 2 end, NOT at network busy end.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSecondsD(13.1));
}

TEST_F(InteractiveDetectorTest, LateLongTaskWithLateFMPDetection) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(2));
  SimulateResourceLoadBegin(
      t0 + base::TimeDelta::FromSecondsD(3.4));  // Request 2 start.
  SimulateResourceLoadBegin(t0 + base::TimeDelta::FromSecondsD(
                                     3.5));  // Request 3 start. Network busy.
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(7),
                   t0 + base::TimeDelta::FromSecondsD(7.1));  // Long task 1.
  SimulateResourceLoadEnd(t0 +
                          base::TimeDelta::FromSeconds(8));  // Network quiet.
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(14),
                   t0 + base::TimeDelta::FromSecondsD(14.1));  // Long task 2.
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(20));
  // TTI reached at long task 1 end, NOT at long task 2 end.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSecondsD(7.1));
}

TEST_F(InteractiveDetectorTest, IntermittentNetworkBusyBlocksTTI) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(2));
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(4));
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(7),
                   t0 + base::TimeDelta::FromSecondsD(7.1));  // Long task 1.
  SimulateResourceLoadBegin(
      t0 + base::TimeDelta::FromSecondsD(7.9));  // Active connections: 2
  // Network busy start.
  SimulateResourceLoadBegin(
      t0 + base::TimeDelta::FromSeconds(8));  // Active connections: 3.
  // Network busy end.
  SimulateResourceLoadEnd(
      t0 + base::TimeDelta::FromSecondsD(8.5));  // Active connections: 2.
  // Network busy start.
  SimulateResourceLoadBegin(
      t0 + base::TimeDelta::FromSeconds(11));  // Active connections: 3.
  // Network busy end.
  SimulateResourceLoadEnd(
      t0 + base::TimeDelta::FromSeconds(12));  // Active connections: 2.
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(14),
                   t0 + base::TimeDelta::FromSecondsD(14.1));  // Long task 2.
  // Run till 5 seconds after long task 2 end.
  RunTillTimestamp((t0 + base::TimeDelta::FromSecondsD(14.1)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // TTI reached at long task 2 end.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSecondsD(14.1));
}

TEST_F(InteractiveDetectorTest, InvalidatingUserInput) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(2));
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(4));
  SimulateInteractiveInvalidatingInput(t0 + base::TimeDelta::FromSeconds(5));
  SimulateLongTask(t0 + base::TimeDelta::FromSeconds(7),
                   t0 + base::TimeDelta::FromSecondsD(7.1));  // Long task 1.
  // Run till 5 seconds after long task 2 end.
  RunTillTimestamp((t0 + base::TimeDelta::FromSecondsD(7.1)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // We still detect interactive time on the blink side even if there is an
  // invalidating user input. Page Load Metrics filters out this value in the
  // browser process for UMA reporting.
  EXPECT_EQ(GetInteractiveTime(), t0 + base::TimeDelta::FromSecondsD(7.1));
  EXPECT_EQ(GetDetector()->GetFirstInvalidatingInputTime(),
            t0 + base::TimeDelta::FromSeconds(5));
}

TEST_F(InteractiveDetectorTest, InvalidatingUserInputClampedAtNavStart) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(2));
  SimulateFMPDetected(
      /* fmp_time */ t0 + base::TimeDelta::FromSeconds(3),
      /* detection_time */ t0 + base::TimeDelta::FromSeconds(4));
  // Invalidating input timestamp is earlier than navigation start.
  SimulateInteractiveInvalidatingInput(t0 - base::TimeDelta::FromSeconds(10));
  // Run till 5 seconds after FMP.
  RunTillTimestamp((t0 + base::TimeDelta::FromSecondsD(7.1)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  EXPECT_EQ(GetInteractiveTime(),
            t0 + base::TimeDelta::FromSeconds(3));  // TTI at FMP.
  // Invalidating input timestamp is clamped at navigation start.
  EXPECT_EQ(GetDetector()->GetFirstInvalidatingInputTime(), t0);
}

TEST_F(InteractiveDetectorTest, InvalidatedFMP) {
  base::TimeTicks t0 = Now();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateInteractiveInvalidatingInput(t0 + base::TimeDelta::FromSeconds(1));
  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(2));
  RunTillTimestamp(t0 +
                   base::TimeDelta::FromSeconds(4));  // FMP Detection time.
  GetDetector()->OnFirstMeaningfulPaintDetected(
      t0 + base::TimeDelta::FromSeconds(3),
      FirstMeaningfulPaintDetector::kHadUserInput);
  // Run till 5 seconds after FMP.
  RunTillTimestamp((t0 + base::TimeDelta::FromSeconds(3)) +
                   base::TimeDelta::FromSecondsD(5.0 + 0.1));
  // Since FMP was invalidated, we do not have TTI or TTI Detection Time.
  EXPECT_EQ(GetInteractiveTime(), base::TimeTicks());
  EXPECT_EQ(
      GetDetector()->GetInteractiveDetectionTime().since_origin().InSecondsF(),
      0.0);
  // Invalidating input timestamp is available.
  EXPECT_EQ(GetDetector()->GetFirstInvalidatingInputTime(),
            t0 + base::TimeDelta::FromSeconds(1));
}

TEST_F(InteractiveDetectorTest, TaskLongerThan5sBlocksTTI) {
  base::TimeTicks t0 = Now();
  GetDetector()->SetNavigationStartTime(t0);

  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(2));
  SimulateFMPDetected(t0 + base::TimeDelta::FromSeconds(3),
                      t0 + base::TimeDelta::FromSeconds(4));

  // Post a task with 6 seconds duration.
  Thread::Current()->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&InteractiveDetectorTest::DummyTaskWithDuration,
                           WTF::Unretained(this), 6.0));

  platform_->RunUntilIdle();

  // We should be able to detect TTI 5s after the end of long task.
  platform_->RunForPeriodSeconds(5.1);
  EXPECT_EQ(GetDetector()->GetInteractiveTime(), GetDummyTaskEndTime());
}

TEST_F(InteractiveDetectorTest, LongTaskAfterTTIDoesNothing) {
  base::TimeTicks t0 = Now();
  GetDetector()->SetNavigationStartTime(t0);

  SimulateDOMContentLoadedEnd(t0 + base::TimeDelta::FromSeconds(2));
  SimulateFMPDetected(t0 + base::TimeDelta::FromSeconds(3),
                      t0 + base::TimeDelta::FromSeconds(4));

  // Long task 1.
  Thread::Current()->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&InteractiveDetectorTest::DummyTaskWithDuration,
                           WTF::Unretained(this), 0.1));

  platform_->RunUntilIdle();

  base::TimeTicks long_task_1_end_time = GetDummyTaskEndTime();
  // We should be able to detect TTI 5s after the end of long task.
  platform_->RunForPeriodSeconds(5.1);
  EXPECT_EQ(GetDetector()->GetInteractiveTime(), long_task_1_end_time);

  // Long task 2.
  Thread::Current()->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&InteractiveDetectorTest::DummyTaskWithDuration,
                           WTF::Unretained(this), 0.1));

  platform_->RunUntilIdle();
  // Wait 5 seconds to see if TTI time changes.
  platform_->RunForPeriodSeconds(5.1);
  // TTI time should not change.
  EXPECT_EQ(GetDetector()->GetInteractiveTime(), long_task_1_end_time);
}

}  // namespace blink
