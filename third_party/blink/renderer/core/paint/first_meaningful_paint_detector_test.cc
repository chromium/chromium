// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/paint/paint_event.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class FirstMeaningfulPaintDetectorTest : public PageTestBase {
 protected:
  void SetUp() override {
    EnablePlatform();
    platform()->AdvanceClock(base::TimeDelta::FromSeconds(1));
    const base::TickClock* test_clock =
        platform()->test_task_runner()->GetMockTickClock();
    FirstMeaningfulPaintDetector::SetTickClockForTesting(test_clock);
    PageTestBase::SetUp();
    GetPaintTiming().SetTickClockForTesting(test_clock);
  }

  void TearDown() override {
    const base::TickClock* clock = base::DefaultTickClock::GetInstance();
    GetPaintTiming().SetTickClockForTesting(clock);
    PageTestBase::TearDown();
    FirstMeaningfulPaintDetector::SetTickClockForTesting(clock);
  }

  base::TimeTicks Now() { return platform()->test_task_runner()->NowTicks(); }

  base::TimeTicks AdvanceClockAndGetTime() {
    platform()->AdvanceClock(base::TimeDelta::FromSeconds(1));
    return Now();
  }

  PaintTiming& GetPaintTiming() { return PaintTiming::From(GetDocument()); }
  FirstMeaningfulPaintDetector& Detector() {
    return GetPaintTiming().GetFirstMeaningfulPaintDetector();
  }

  void SimulateLayoutAndPaint(int new_elements) {
    platform()->AdvanceClock(base::TimeDelta::FromMilliseconds(1));
    StringBuilder builder;
    for (int i = 0; i < new_elements; i++)
      builder.Append("<span>a</span>");
    GetDocument().write(builder.ToString());
    GetDocument().UpdateStyleAndLayout();
    Detector().NotifyPaint();
  }

  void SimulateNetworkStable() {
    GetDocument().SetParsingState(Document::kFinishedParsing);
    Detector().OnNetwork2Quiet();
  }

  void SimulateUserInput() { Detector().NotifyInputEvent(); }

  void ClearFirstPaintSwapPromise() {
    platform()->AdvanceClock(base::TimeDelta::FromMilliseconds(1));
    GetPaintTiming().ReportSwapTime(
        PaintEvent::kFirstPaint, WebWidgetClient::SwapResult::kDidSwap, Now());
  }

  void ClearFirstContentfulPaintSwapPromise() {
    platform()->AdvanceClock(base::TimeDelta::FromMilliseconds(1));
    GetPaintTiming().ReportSwapTime(PaintEvent::kFirstContentfulPaint,
                                    WebWidgetClient::SwapResult::kDidSwap,
                                    Now());
  }

  void ClearProvisionalFirstMeaningfulPaintSwapPromise() {
    platform()->AdvanceClock(base::TimeDelta::FromMilliseconds(1));
    ClearProvisionalFirstMeaningfulPaintSwapPromise(Now());
  }

  void ClearProvisionalFirstMeaningfulPaintSwapPromise(
      base::TimeTicks timestamp) {
    Detector().ReportSwapTime(PaintEvent::kProvisionalFirstMeaningfulPaint,
                              WebWidgetClient::SwapResult::kDidSwap, timestamp);
  }

  unsigned OutstandingDetectorSwapPromiseCount() {
    return Detector().outstanding_swap_promise_count_;
  }

  void MarkFirstContentfulPaintAndClearSwapPromise() {
    GetPaintTiming().MarkFirstContentfulPaint();
    ClearFirstContentfulPaintSwapPromise();
  }

  void MarkFirstPaintAndClearSwapPromise() {
    GetPaintTiming().MarkFirstPaint();
    ClearFirstPaintSwapPromise();
  }
};

TEST_F(FirstMeaningfulPaintDetectorTest, NoFirstPaint) {
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 0U);
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest, OneLayout) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  base::TimeTicks after_paint = AdvanceClockAndGetTime();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  SimulateNetworkStable();
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_paint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantSecond) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  base::TimeTicks after_layout1 = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  base::TimeTicks after_layout2 = AdvanceClockAndGetTime();
  SimulateNetworkStable();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), after_layout1);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_layout2);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantFirst) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  base::TimeTicks after_layout1 = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 0U);
  SimulateNetworkStable();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstPaintRendered());
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_layout1);
}

TEST_F(FirstMeaningfulPaintDetectorTest, FirstMeaningfulPaintCandidate) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(),
            base::TimeTicks());
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  base::TimeTicks after_paint = AdvanceClockAndGetTime();
  // The first candidate gets ignored.
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(),
            base::TimeTicks());
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  // The second candidate gets reported.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintCandidate(), after_paint);
  base::TimeTicks candidate = GetPaintTiming().FirstMeaningfulPaintCandidate();
  // The third candidate gets ignored since we already saw the first candidate.
  SimulateLayoutAndPaint(20);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), candidate);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       OnlyOneFirstMeaningfulPaintCandidateBeforeNetworkStable) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(),
            base::TimeTicks());
  base::TimeTicks before_paint = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  // The first candidate is initially ignored.
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(),
            base::TimeTicks());
  SimulateNetworkStable();
  // The networkStable then promotes the first candidate.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintCandidate(), before_paint);
  base::TimeTicks candidate = GetPaintTiming().FirstMeaningfulPaintCandidate();
  // The second candidate is then ignored.
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 0U);
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), candidate);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       NetworkStableBeforeFirstContentfulPaint) {
  MarkFirstPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateNetworkStable();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       FirstMeaningfulPaintShouldNotBeBeforeFirstContentfulPaint) {
  MarkFirstPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  platform()->AdvanceClock(base::TimeDelta::FromMilliseconds(1));
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateNetworkStable();
  EXPECT_GE(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstContentfulPaint());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       FirstMeaningfulPaintAfterUserInteraction) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateUserInput();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest, UserInteractionBeforeFirstPaint) {
  SimulateUserInput();
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  SimulateNetworkStable();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       WaitForSingleOutstandingSwapPromiseAfterNetworkStable) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       WaitForMultipleOutstandingSwapPromisesAfterNetworkStable) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  platform()->AdvanceClock(base::TimeDelta::FromMilliseconds(1));
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 2U);
  // Having outstanding swap promises should defer setting FMP.
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  // Clearing the first swap promise should have no effect on FMP.
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  base::TimeTicks after_first_swap = AdvanceClockAndGetTime();
  // Clearing the last outstanding swap promise should set FMP.
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 0U);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), after_first_swap);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       WaitForFirstContentfulPaintSwapAfterNetworkStable) {
  MarkFirstPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  platform()->AdvanceClock(base::TimeDelta::FromMilliseconds(1));
  GetPaintTiming().MarkFirstContentfulPaint();
  // FCP > FMP candidate, but still waiting for FCP swap.
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  // Trigger notifying the detector about the FCP swap.
  ClearFirstContentfulPaintSwapPromise();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstContentfulPaint());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       ProvisionalTimestampChangesAfterNetworkQuietWithOutstandingSwapPromise) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);

  // Simulate network stable so provisional FMP will be set on next layout.
  base::TimeTicks pre_stable_timestamp = AdvanceClockAndGetTime();
  platform()->AdvanceClock(base::TimeDelta::FromMilliseconds(1));
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());

  // Force another FMP candidate while there is a pending swap promise and the
  // FMP non-swap timestamp is set.
  platform()->AdvanceClock(base::TimeDelta::FromMilliseconds(1));
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);

  // Simulate a delay in receiving the SwapPromise timestamp. Clearing this
  // SwapPromise will set FMP, and this will crash if the new provisional
  // non-swap timestamp is used.
  ClearProvisionalFirstMeaningfulPaintSwapPromise(pre_stable_timestamp);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 0U);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), pre_stable_timestamp);
}

}  // namespace blink
