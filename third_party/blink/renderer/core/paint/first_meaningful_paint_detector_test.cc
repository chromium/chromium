// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/renderer/core/paint/paint_event.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class FirstMeaningfulPaintDetectorTest : public PageTestBase {
 protected:
  void SetUp() override {
    platform_->AdvanceClock(TimeDelta::FromSeconds(1));
    PageTestBase::SetUp();
  }

  TimeTicks AdvanceClockAndGetTime() {
    platform_->AdvanceClock(TimeDelta::FromSeconds(1));
    return CurrentTimeTicks();
  }

  PaintTiming& GetPaintTiming() { return PaintTiming::From(GetDocument()); }
  FirstMeaningfulPaintDetector& Detector() {
    return GetPaintTiming().GetFirstMeaningfulPaintDetector();
  }

  void SimulateLayoutAndPaint(int new_elements) {
    platform_->AdvanceClock(TimeDelta::FromMilliseconds(1));
    StringBuilder builder;
    for (int i = 0; i < new_elements; i++)
      builder.Append("<span>a</span>");
    GetDocument().write(builder.ToString());
    GetDocument().UpdateStyleAndLayout();
    Detector().NotifyPaint();
  }

  void SimulateNetworkStable() {
    GetDocument().SetParsingState(Document::kFinishedParsing);
    Detector().OnNetwork0Quiet();
    Detector().OnNetwork2Quiet();
  }

  void SimulateNetwork0Quiet() {
    GetDocument().SetParsingState(Document::kFinishedParsing);
    Detector().OnNetwork0Quiet();
  }

  void SimulateNetwork2Quiet() {
    GetDocument().SetParsingState(Document::kFinishedParsing);
    Detector().OnNetwork2Quiet();
  }

  void SimulateUserInput() { Detector().NotifyInputEvent(); }

  void ClearFirstPaintSwapPromise() {
    platform_->AdvanceClock(TimeDelta::FromMilliseconds(1));
    GetPaintTiming().ReportSwapTime(PaintEvent::kFirstPaint,
                                    WebLayerTreeView::SwapResult::kDidSwap,
                                    CurrentTimeTicks());
  }

  void ClearFirstContentfulPaintSwapPromise() {
    platform_->AdvanceClock(TimeDelta::FromMilliseconds(1));
    GetPaintTiming().ReportSwapTime(PaintEvent::kFirstContentfulPaint,
                                    WebLayerTreeView::SwapResult::kDidSwap,
                                    CurrentTimeTicks());
  }

  void ClearProvisionalFirstMeaningfulPaintSwapPromise() {
    platform_->AdvanceClock(TimeDelta::FromMilliseconds(1));
    ClearProvisionalFirstMeaningfulPaintSwapPromise(CurrentTimeTicks());
  }

  void ClearProvisionalFirstMeaningfulPaintSwapPromise(
      base::TimeTicks timestamp) {
    Detector().ReportSwapTime(PaintEvent::kProvisionalFirstMeaningfulPaint,
                              WebLayerTreeView::SwapResult::kDidSwap,
                              timestamp);
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

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

TEST_F(FirstMeaningfulPaintDetectorTest, NoFirstPaint) {
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 0U);
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest, OneLayout) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  TimeTicks after_paint = AdvanceClockAndGetTime();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  SimulateNetworkStable();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintRendered(),
            GetPaintTiming().FirstPaintRendered());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaintRendered(), after_paint);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_paint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantSecond) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  TimeTicks after_layout1 = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  TimeTicks after_layout2 = AdvanceClockAndGetTime();
  SimulateNetworkStable();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintRendered(), after_layout1);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), after_layout1);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaintRendered(), after_layout2);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_layout2);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantFirst) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  TimeTicks after_layout1 = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 0U);
  SimulateNetworkStable();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintRendered(),
            GetPaintTiming().FirstPaintRendered());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstPaintRendered());
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaintRendered(), after_layout1);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_layout1);
}

TEST_F(FirstMeaningfulPaintDetectorTest, FirstMeaningfulPaintCandidate) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), TimeTicks());
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  TimeTicks after_paint = AdvanceClockAndGetTime();
  // The first candidate gets ignored.
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), TimeTicks());
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  // The second candidate gets reported.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintCandidate(), after_paint);
  TimeTicks candidate = GetPaintTiming().FirstMeaningfulPaintCandidate();
  // The third candidate gets ignored since we already saw the first candidate.
  SimulateLayoutAndPaint(20);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), candidate);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       OnlyOneFirstMeaningfulPaintCandidateBeforeNetworkStable) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), TimeTicks());
  TimeTicks before_paint = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  // The first candidate is initially ignored.
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), TimeTicks());
  SimulateNetworkStable();
  // The networkStable then promotes the first candidate.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintCandidate(), before_paint);
  TimeTicks candidate = GetPaintTiming().FirstMeaningfulPaintCandidate();
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
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateNetworkStable();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       FirstMeaningfulPaintShouldNotBeBeforeFirstContentfulPaint) {
  MarkFirstPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  platform_->AdvanceClock(TimeDelta::FromMilliseconds(1));
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateNetworkStable();
  EXPECT_GE(GetPaintTiming().FirstMeaningfulPaintRendered(),
            GetPaintTiming().FirstContentfulPaintRendered());
  EXPECT_GE(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstContentfulPaint());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
}

TEST_F(FirstMeaningfulPaintDetectorTest, Network2QuietThen0Quiet) {
  MarkFirstContentfulPaintAndClearSwapPromise();

  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  TimeTicks after_first_paint = AdvanceClockAndGetTime();
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  TimeTicks after_first_paint_swap = AdvanceClockAndGetTime();
  SimulateNetwork2Quiet();

  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 0U);
  SimulateNetwork0Quiet();

  // The first paint is FirstMeaningfulPaint.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaintRendered(), after_first_paint);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), after_first_paint);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_first_paint_swap);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
}

TEST_F(FirstMeaningfulPaintDetectorTest, Network0QuietThen2Quiet) {
  MarkFirstContentfulPaintAndClearSwapPromise();

  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  TimeTicks after_first_paint = AdvanceClockAndGetTime();
  SimulateNetwork0Quiet();

  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  TimeTicks after_second_paint = AdvanceClockAndGetTime();
  SimulateNetwork2Quiet();

  // The second paint is FirstMeaningfulPaint.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintRendered(), after_first_paint);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), after_first_paint);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaintRendered(),
            after_second_paint);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_second_paint);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       FirstMeaningfulPaintAfterUserInteraction) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateUserInput();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest, UserInteractionBeforeFirstPaint) {
  SimulateUserInput();
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  SimulateNetworkStable();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       WaitForSingleOutstandingSwapPromiseAfterNetworkStable) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       WaitForMultipleOutstandingSwapPromisesAfterNetworkStable) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  platform_->AdvanceClock(TimeDelta::FromMilliseconds(1));
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 2U);
  // Having outstanding swap promises should defer setting FMP.
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  // Clearing the first swap promise should have no effect on FMP.
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  TimeTicks after_first_swap = AdvanceClockAndGetTime();
  // Clearing the last outstanding swap promise should set FMP.
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 0U);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), after_first_swap);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       WaitForFirstContentfulPaintSwapAfterNetworkStable) {
  MarkFirstPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintSwapPromise();
  TimeTicks after_first_meaningful_paint_candidate = AdvanceClockAndGetTime();
  platform_->AdvanceClock(TimeDelta::FromMilliseconds(1));
  GetPaintTiming().MarkFirstContentfulPaint();
  // FCP > FMP candidate, but still waiting for FCP swap.
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  // Trigger notifying the detector about the FCP swap.
  ClearFirstContentfulPaintSwapPromise();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintRendered(),
            GetPaintTiming().FirstContentfulPaintRendered());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstContentfulPaint());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintRendered(),
            after_first_meaningful_paint_candidate);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstMeaningfulPaintRendered());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       ProvisionalTimestampChangesAfterNetworkQuietWithOutstandingSwapPromise) {
  MarkFirstContentfulPaintAndClearSwapPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);

  // Simulate only network 2-quiet so provisional FMP will be set on next
  // layout.
  TimeTicks pre_stable_timestamp = AdvanceClockAndGetTime();
  platform_->AdvanceClock(TimeDelta::FromMilliseconds(1));
  SimulateNetwork2Quiet();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());

  // Force another FMP candidate while there is a pending swap promise and the
  // network 2-quiet FMP non-swap timestamp is set.
  platform_->AdvanceClock(TimeDelta::FromMilliseconds(1));
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 1U);

  // Simulate a delay in receiving the SwapPromise timestamp. Clearing this
  // SwapPromise will set FMP, and this will crash if the new provisional
  // non-swap timestamp is used.
  ClearProvisionalFirstMeaningfulPaintSwapPromise(pre_stable_timestamp);
  EXPECT_EQ(OutstandingDetectorSwapPromiseCount(), 0U);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintRendered(), TimeTicks());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), pre_stable_timestamp);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaintRendered(),
            pre_stable_timestamp);
}

}  // namespace blink
