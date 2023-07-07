// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_paint_invalidator.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_and_raster_invalidation_test.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using ::testing::UnorderedElementsAre;

class BoxPaintInvalidatorTest : public PaintAndRasterInvalidationTest {
 public:
  BoxPaintInvalidatorTest() = default;

 protected:
  PaintInvalidationReason ComputePaintInvalidationReason(
      LayoutBox& box,
      const PhysicalOffset& old_paint_offset) {
    PaintInvalidatorContext context;
    context.old_paint_offset = old_paint_offset;
    fragment_data_->SetPaintOffset(box.FirstFragment().PaintOffset());
    context.fragment_data = fragment_data_;
    return BoxPaintInvalidator(box, context).ComputePaintInvalidationReason();
  }

  // This applies when the target is set to meet conditions that we should do
  // full paint invalidation instead of incremental invalidation on geometry
  // change.
  void ExpectFullPaintInvalidationOnGeometryChange(const char* test_title) {
    SCOPED_TRACE(test_title);

    UpdateAllLifecyclePhasesForTest();
    auto& target = *GetDocument().getElementById(AtomicString("target"));
    auto& box = *target.GetLayoutBox();
    auto paint_offset = box.FirstFragment().PaintOffset();
    box.SetShouldCheckForPaintInvalidation();

    // No geometry change.
    EXPECT_EQ(PaintInvalidationReason::kNone,
              ComputePaintInvalidationReason(box, paint_offset));

    target.setAttribute(
        html_names::kStyleAttr,
        target.getAttribute(html_names::kStyleAttr) + "; width: 200px");
    GetDocument().View()->UpdateLifecycleToLayoutClean(
        DocumentUpdateReason::kTest);

    EXPECT_EQ(PaintInvalidationReason::kLayout,
              ComputePaintInvalidationReason(box, paint_offset));
  }

  void SetUpHTML() {
    SetBodyInnerHTML(R"HTML(
      <style>
        body {
          margin: 0;
          height: 0;
        }
        ::-webkit-scrollbar { display: none }
        #target {
          width: 50px;
          height: 100px;
          transform-origin: 0 0;
        }
        .background {
          background: blue;
        }
        .border {
          border-width: 20px 10px;
          border-style: solid;
          border-color: red;
        }
      </style>
      <div id='target' class='border'></div>
    )HTML");
  }

 private:
  Persistent<FragmentData> fragment_data_ =
      MakeGarbageCollected<FragmentData>();
};

INSTANTIATE_PAINT_TEST_SUITE_P(BoxPaintInvalidatorTest);

// Paint invalidation for empty content is needed for updating composited layer
// bounds for correct composited hit testing. It won't cause raster invalidation
// (tested in paint_and_raster_invalidation_test.cc).
TEST_P(BoxPaintInvalidatorTest, ComputePaintInvalidationReasonEmptyContent) {
  SetUpHTML();
  auto& target = *GetDocument().getElementById(AtomicString("target"));
  auto& box = *target.GetLayoutBox();
  // Remove border.
  target.setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();

  box.SetShouldCheckForPaintInvalidation();
  auto paint_offset = box.FirstFragment().PaintOffset();

  // No geometry change.
  EXPECT_EQ(PaintInvalidationReason::kNone,
            ComputePaintInvalidationReason(box, paint_offset));

  // Paint offset change.
  auto old_paint_offset = paint_offset + PhysicalOffset(10, 20);
  EXPECT_EQ(PaintInvalidationReason::kLayout,
            ComputePaintInvalidationReason(box, old_paint_offset));

  // Size change.
  target.setAttribute(html_names::kStyleAttr, AtomicString("width: 200px"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            ComputePaintInvalidationReason(box, paint_offset));
}

TEST_P(BoxPaintInvalidatorTest, ComputePaintInvalidationReasonBasic) {
  SetUpHTML();
  auto& target = *GetDocument().getElementById(AtomicString("target"));
  auto& box = *target.GetLayoutBox();
  // Remove border.
  target.setAttribute(html_names::kClassAttr, g_empty_atom);
  target.setAttribute(html_names::kStyleAttr, AtomicString("background: blue"));
  UpdateAllLifecyclePhasesForTest();

  box.SetShouldCheckForPaintInvalidation();
  auto paint_offset = box.FirstFragment().PaintOffset();
  EXPECT_EQ(PhysicalOffset(), paint_offset);

  // No geometry change.
  EXPECT_EQ(PaintInvalidationReason::kNone,
            ComputePaintInvalidationReason(box, paint_offset));

  // Size change.
  target.setAttribute(html_names::kStyleAttr,
                      AtomicString("background: blue; width: 200px"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            ComputePaintInvalidationReason(box, paint_offset));

  // Add visual overflow.
  target.setAttribute(
      html_names::kStyleAttr,
      AtomicString("background: blue; width: 200px; outline: 5px solid red"));
  UpdateAllLifecyclePhasesForTest();

  // Size change with visual overflow.
  target.setAttribute(
      html_names::kStyleAttr,
      AtomicString("background: blue; width: 100px; outline: 5px solid red"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  EXPECT_EQ(PaintInvalidationReason::kLayout,
            ComputePaintInvalidationReason(box, paint_offset));

  // Computed kLayout has higher priority than the non-geometry paint
  // invalidation reason on the LayoutBox.
  box.SetShouldDoFullPaintInvalidationWithoutLayoutChange(
      PaintInvalidationReason::kStyle);
  EXPECT_EQ(PaintInvalidationReason::kLayout,
            ComputePaintInvalidationReason(box, paint_offset));

  // If the LayoutBox has a geometry paint invalidation reason, the reason is
  // returned directly without checking geometry change.
  box.SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kSVGResource);
  EXPECT_EQ(PaintInvalidationReason::kSVGResource,
            ComputePaintInvalidationReason(box, paint_offset));
}

TEST_P(BoxPaintInvalidatorTest,
       InvalidateLineBoxHitTestOnCompositingStyleChange) {
  ScopedPaintUnderInvalidationCheckingForTest under_invalidation_checking(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 100px;
        height: 100px;
        touch-action: none;
      }
    </style>
    <div id="target" style="will-change: transform;">a<br>b</div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  auto& target = *GetDocument().getElementById(AtomicString("target"));
  target.setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  // This test passes if no underinvalidation occurs.
}

TEST_P(BoxPaintInvalidatorTest, ComputePaintInvalidationReasonOtherCases) {
  SetUpHTML();
  auto& target = *GetDocument().getElementById(AtomicString("target"));

  // The target initially has border.
  ExpectFullPaintInvalidationOnGeometryChange("With border");

  // Clear border, set background.
  target.setAttribute(html_names::kClassAttr, AtomicString("background"));
  target.setAttribute(html_names::kStyleAttr,
                      AtomicString("border-radius: 5px"));
  ExpectFullPaintInvalidationOnGeometryChange("With border-radius");

  target.setAttribute(html_names::kStyleAttr,
                      AtomicString("-webkit-mask: url(#)"));
  ExpectFullPaintInvalidationOnGeometryChange("With mask");

  target.setAttribute(html_names::kStyleAttr,
                      AtomicString("filter: blur(5px)"));
  ExpectFullPaintInvalidationOnGeometryChange("With filter");

  target.setAttribute(html_names::kStyleAttr,
                      AtomicString("box-shadow: inset 3px 2px"));
  ExpectFullPaintInvalidationOnGeometryChange("With box-shadow");

  target.setAttribute(html_names::kStyleAttr,
                      AtomicString("clip-path: circle(50% at 0 50%)"));
  ExpectFullPaintInvalidationOnGeometryChange("With clip-path");
}

TEST_P(BoxPaintInvalidatorTest, ComputePaintInvalidationReasonOutline) {
  SetUpHTML();
  auto& target = *GetDocument().getElementById(AtomicString("target"));
  auto* object = target.GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target.setAttribute(html_names::kStyleAttr,
                      AtomicString("outline: 2px solid blue;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  object->Id(), object->DebugName(), gfx::Rect(0, 0, 72, 142),
                  PaintInvalidationReason::kLayout}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target.setAttribute(html_names::kStyleAttr,
                      AtomicString("outline: 2px solid blue; width: 100px;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  object->Id(), object->DebugName(), gfx::Rect(0, 0, 122, 142),
                  PaintInvalidationReason::kLayout}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, InvalidateHitTestOnCompositingStyleChange) {
  ScopedPaintUnderInvalidationCheckingForTest under_invalidation_checking(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 400px;
        height: 300px;
        overflow: hidden;
        touch-action: none;
      }
    </style>
    <div id="target" style="will-change: transform;"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  auto& target = *GetDocument().getElementById(AtomicString("target"));
  target.setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  // This test passes if no under-invalidation occurs.
}

}  // namespace blink
