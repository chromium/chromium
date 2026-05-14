// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/sticky_ad_detector.h"

#include "components/viz/common/frame_timing_details.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/paint_event.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class StickyAdDetectorTest : public RenderingTest {
 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  void SimulateFirstContentfulPaint() {
    viz::FrameTimingDetails details;
    details.presentation_feedback.timestamp = base::TimeTicks::Now();
    PaintTiming::From(GetDocument())
        .ReportPresentationTime(PaintEvent::kFirstContentfulPaint,
                                base::TimeTicks(), details);
  }
};

// Regression test: MaybeFireDetection should not crash when the document
// lifecycle has not yet reached kPrePaintClean.
TEST_F(StickyAdDetectorTest, MaybeFireDetectionSkipsWhenNotPrePaintClean) {
  SetBodyInnerHTML("<div>content</div>");
  UpdateAllLifecyclePhasesForTest();
  SimulateFirstContentfulPaint();
  ASSERT_FALSE(
      PaintTiming::From(GetDocument()).FirstContentfulPaint().is_null());

  GetDocument().View()->SetNeedsLayout();
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  ASSERT_EQ(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);

  // Without the lifecycle guard this would DCHECK in PaintLayer::HitTestLayer.
  StickyAdDetector detector;
  detector.MaybeFireDetection(&GetDocument().GetFrame()->LocalFrameRoot());
}

}  // namespace blink
