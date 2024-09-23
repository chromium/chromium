// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/first_meaningful_paint_detector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/paint/paint_event.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class FirstMeaningfulPaintDetectorTest : public PageTestBase {
 public:
  FirstMeaningfulPaintDetectorTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    EnablePlatform();
    AdvanceClock(base::Seconds(1));
    const base::TickClock* test_clock = platform()->GetTickClock();
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

  base::TimeTicks Now() { return platform()->NowTicks(); }

  base::TimeTicks AdvanceClockAndGetTime() {
    AdvanceClock(base::Seconds(1));
    return Now();
  }

  PaintTiming& GetPaintTiming() { return PaintTiming::From(GetDocument()); }
  FirstMeaningfulPaintDetector& Detector() {
    return GetPaintTiming().GetFirstMeaningfulPaintDetector();
  }

  void SimulateLayoutAndPaint(int new_elements) {
    AdvanceClock(base::Milliseconds(1));
    StringBuilder builder;
    for (int i = 0; i < new_elements; i++)
      builder.Append("<span>a</span>");
    GetDocument().write(builder.ToString());
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    Detector().NotifyPaint();
  }

  void SimulateNetworkStable() {
    GetDocument().SetParsingState(Document::kFinishedParsing);
    Detector().OnNetwork2Quiet();
  }

  void SimulateUserInput() { Detector().NotifyInputEvent(); }

  void ClearFirstPaintPresentationPromise() {
    AdvanceClock(base::Milliseconds(1));
    viz::FrameTimingDetails presentation_details;
    presentation_details.presentation_feedback.timestamp = Now();
    GetPaintTiming().ReportPresentationTime(PaintEvent::kFirstPaint,
                                            presentation_details);
  }

  void ClearFirstContentfulPaintPresentationPromise() {
    AdvanceClock(base::Milliseconds(1));
    viz::FrameTimingDetails presentation_details;
    presentation_details.presentation_feedback.timestamp = Now();
    GetPaintTiming().ReportPresentationTime(PaintEvent::kFirstContentfulPaint,
                                            presentation_details);
  }

  void ClearProvisionalFirstMeaningfulPaintPresentationPromise() {
    AdvanceClock(base::Milliseconds(1));
    ClearProvisionalFirstMeaningfulPaintPresentationPromise(Now());
  }

  void ClearProvisionalFirstMeaningfulPaintPresentationPromise(
      base::TimeTicks timestamp) {
    viz::FrameTimingDetails presentation_details;
    presentation_details.presentation_feedback.timestamp = timestamp;
    Detector().ReportPresentationTime(
        PaintEvent::kProvisionalFirstMeaningfulPaint, presentation_details);
  }

  unsigned OutstandingDetectorPresentationPromiseCount() {
    return Detector().outstanding_presentation_promise_count_;
  }

  void MarkFirstContentfulPaintAndClearPresentationPromise() {
    GetPaintTiming().MarkFirstContentfulPaint();
    ClearFirstContentfulPaintPresentationPromise();
  }

  void MarkFirstPaintAndClearPresentationPromise() {
    GetPaintTiming().MarkFirstPaint();
    ClearFirstPaintPresentationPromise();
  }
};

TEST_F(FirstMeaningfulPaintDetectorTest, NoFirstPaint) {
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 0U);
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest, OneLayout) {
  MarkFirstContentfulPaintAndClearPresentationPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  base::TimeTicks after_paint = AdvanceClockAndGetTime();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  SimulateNetworkStable();
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_paint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantSecond) {
  MarkFirstContentfulPaintAndClearPresentationPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  base::TimeTicks after_layout1 = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  base::TimeTicks after_layout2 = AdvanceClockAndGetTime();
  SimulateNetworkStable();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), after_layout1);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_layout2);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantFirst) {
  MarkFirstContentfulPaintAndClearPresentationPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  base::TimeTicks after_layout1 = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 0U);
  SimulateNetworkStable();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstPaintRendered());
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_layout1);
}

TEST_F(FirstMeaningfulPaintDetectorTest, FirstMeaningfulPaintCandidate) {
  MarkFirstContentfulPaintAndClearPresentationPromise();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(),
            base::TimeTicks());
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  base::TimeTicks after_paint = AdvanceClockAndGetTime();
  // The first candidate gets ignored.
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(),
            base::TimeTicks());
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  // The second candidate gets reported.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintCandidate(), after_paint);
  base::TimeTicks candidate = GetPaintTiming().FirstMeaningfulPaintCandidate();
  // The third candidate gets ignored since we already saw the first candidate.
  SimulateLayoutAndPaint(20);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), candidate);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       OnlyOneFirstMeaningfulPaintCandidateBeforeNetworkStable) {
  MarkFirstContentfulPaintAndClearPresentationPromise();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(),
            base::TimeTicks());
  base::TimeTicks before_paint = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  // The first candidate is initially ignored.
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(),
            base::TimeTicks());
  SimulateNetworkStable();
  // The networkStable then promotes the first candidate.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintCandidate(), before_paint);
  base::TimeTicks candidate = GetPaintTiming().FirstMeaningfulPaintCandidate();
  // The second candidate is then ignored.
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 0U);
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), candidate);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       NetworkStableBeforeFirstContentfulPaint) {
  MarkFirstPaintAndClearPresentationPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  MarkFirstContentfulPaintAndClearPresentationPromise();
  SimulateNetworkStable();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       FirstMeaningfulPaintShouldNotBeBeforeFirstContentfulPaint) {
  MarkFirstPaintAndClearPresentationPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  AdvanceClock(base::Milliseconds(1));
  MarkFirstContentfulPaintAndClearPresentationPromise();
  SimulateNetworkStable();
  EXPECT_GE(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstContentfulPaintIgnoringSoftNavigations());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       FirstMeaningfulPaintAfterUserInteraction) {
  MarkFirstContentfulPaintAndClearPresentationPromise();
  SimulateUserInput();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest, UserInteractionBeforeFirstPaint) {
  SimulateUserInput();
  MarkFirstContentfulPaintAndClearPresentationPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  SimulateNetworkStable();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       WaitForSingleOutstandingPresentationPromiseAfterNetworkStable) {
  MarkFirstContentfulPaintAndClearPresentationPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       WaitForMultipleOutstandingPresentationPromisesAfterNetworkStable) {
  MarkFirstContentfulPaintAndClearPresentationPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  AdvanceClock(base::Milliseconds(1));
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 2U);
  // Having outstanding presentation promises should defer setting FMP.
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  // Clearing the first presentation promise should have no effect on FMP.
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  base::TimeTicks after_first_presentation = AdvanceClockAndGetTime();
  // Clearing the last outstanding presentation promise should set FMP.
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 0U);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), after_first_presentation);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       WaitForFirstContentfulPaintPresentationpAfterNetworkStable) {
  MarkFirstPaintAndClearPresentationPromise();
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);
  ClearProvisionalFirstMeaningfulPaintPresentationPromise();
  AdvanceClock(base::Milliseconds(1));
  GetPaintTiming().MarkFirstContentfulPaint();
  // FCP > FMP candidate, but still waiting for FCP presentation.
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  // Trigger notifying the detector about the FCP presentation.
  ClearFirstContentfulPaintPresentationPromise();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstContentfulPaintIgnoringSoftNavigations());
}

TEST_F(
    FirstMeaningfulPaintDetectorTest,
    ProvisionalTimestampChangesAfterNetworkQuietWithOutstandingPresentationPromise) {
  MarkFirstContentfulPaintAndClearPresentationPromise();
  SimulateLayoutAndPaint(1);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);

  // Simulate network stable so provisional FMP will be set on next layout.
  base::TimeTicks pre_stable_timestamp = AdvanceClockAndGetTime();
  AdvanceClock(base::Milliseconds(1));
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());

  // Force another FMP candidate while there is a pending presentation promise
  // and the FMP non-presentation timestamp is set.
  AdvanceClock(base::Milliseconds(1));
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 1U);

  // Simulate a delay in receiving the PresentationPromise timestamp. Clearing
  // this PresentationPromise will set FMP, and this will crash if the new
  // provisional non-presentation timestamp is used.
  ClearProvisionalFirstMeaningfulPaintPresentationPromise(pre_stable_timestamp);
  EXPECT_EQ(OutstandingDetectorPresentationPromiseCount(), 0U);
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), base::TimeTicks());
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), pre_stable_timestamp);
}

}  // namespace blink
