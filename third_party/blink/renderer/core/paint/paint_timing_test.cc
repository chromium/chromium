// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_timing.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class PaintTimingTest : public RenderingTest {
 public:
  PaintTimingTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(PaintTimingTest, InViewFrame) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kPaintTimingNoOutOfViewFrames, true);

  SetBodyInnerHTML("<iframe></iframe>");
  SetChildFrameHTML("ABC");
  UpdateAllLifecyclePhasesForTest();

  PaintTiming& paint_timing = PaintTiming::From(ChildDocument());
  paint_timing.ReportPresentationTime(PaintEvent::kFirstContentfulPaint,
                                      WebSwapResult::kDidSwap,
                                      base::TimeTicks::Now());
  EXPECT_FALSE(paint_timing.FirstContentfulPaint().is_null());
}

TEST_F(PaintTimingTest, ExcludeOutOfViewFrame) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kPaintTimingNoOutOfViewFrames, true);

  SetBodyInnerHTML(
      "<iframe style='position: absolute; top: 20000px'></iframe>");
  SetChildFrameHTML("ABC");
  UpdateAllLifecyclePhasesForTest();

  // Should not report FCP for the out-of-view frame.
  PaintTiming& paint_timing = PaintTiming::From(ChildDocument());
  paint_timing.ReportPresentationTime(PaintEvent::kFirstContentfulPaint,
                                      WebSwapResult::kDidSwap,
                                      base::TimeTicks::Now());
  EXPECT_TRUE(paint_timing.FirstContentfulPaint().is_null());
}

TEST_F(PaintTimingTest, ExcludeOutOfViewFrameScrolledIntoView) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kPaintTimingNoOutOfViewFrames, true);

  SetBodyInnerHTML(
      "<iframe style='position: absolute; top: 20000px'></iframe>");
  SetChildFrameHTML("ABC");
  UpdateAllLifecyclePhasesForTest();

  // Should not report FCP even if the iframe scrolls into view and get painted
  // because the iframe is initially out-of-view.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 20000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  PaintTiming& paint_timing = PaintTiming::From(ChildDocument());
  paint_timing.ReportPresentationTime(PaintEvent::kFirstContentfulPaint,
                                      WebSwapResult::kDidSwap,
                                      base::TimeTicks::Now());
  EXPECT_TRUE(paint_timing.FirstContentfulPaint().is_null());
}

TEST_F(PaintTimingTest, IncludeOutOfViewFrame) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kPaintTimingNoOutOfViewFrames, false);

  SetBodyInnerHTML(
      "<iframe style='position: absolute; top: 20000px'></iframe>");
  SetChildFrameHTML("ABC");
  UpdateAllLifecyclePhasesForTest();

  PaintTiming& paint_timing = PaintTiming::From(ChildDocument());
  paint_timing.ReportPresentationTime(PaintEvent::kFirstContentfulPaint,
                                      WebSwapResult::kDidSwap,
                                      base::TimeTicks::Now());
  EXPECT_FALSE(paint_timing.FirstContentfulPaint().is_null());
}

}  // namespace blink
