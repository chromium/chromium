// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_paint_invalidator.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class BoxPaintInvalidatorTest : public PaintControllerPaintTest {
 public:
  BoxPaintInvalidatorTest()
      : PaintControllerPaintTest(
            MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  PaintInvalidationReason ComputePaintInvalidationReason(
      const LayoutBox& box,
      const IntRect& old_visual_rect,
      const PhysicalOffset& old_paint_offset) {
    FragmentData fragment_data;
    PaintInvalidatorContext context;
    context.old_visual_rect = old_visual_rect;
    context.old_paint_offset = old_paint_offset;
    fragment_data_.SetVisualRect(box.FirstFragment().VisualRect());
    fragment_data_.SetPaintOffset(box.FirstFragment().PaintOffset());
    context.fragment_data = &fragment_data_;
    return BoxPaintInvalidator(box, context).ComputePaintInvalidationReason();
  }

  // Accepts old_paint_offset as an IntPoint for convenience of passing it
  // based on visual rect offset.
  PaintInvalidationReason ComputePaintInvalidationReason(
      const LayoutBox& box,
      const IntRect& old_visual_rect,
      const IntPoint& old_paint_offset) {
    return ComputePaintInvalidationReason(box, old_visual_rect,
                                          PhysicalOffset(old_paint_offset));
  }

  // This applies when the target is set to meet conditions that we should do
  // full paint invalidation instead of incremental invalidation on geometry
  // change.
  void ExpectFullPaintInvalidationOnGeometryChange(const char* test_title) {
    SCOPED_TRACE(test_title);

    UpdateAllLifecyclePhasesForTest();
    auto& target = *GetDocument().getElementById("target");
    auto& box = *ToLayoutBox(target.GetLayoutObject());
    auto visual_rect = box.FirstFragment().VisualRect();
    auto paint_offset = box.FirstFragment().PaintOffset();

    // No geometry change.
    EXPECT_EQ(PaintInvalidationReason::kNone,
              ComputePaintInvalidationReason(box, visual_rect, paint_offset));

    target.setAttribute(
        html_names::kStyleAttr,
        target.getAttribute(html_names::kStyleAttr) + "; width: 200px");
    GetDocument().View()->UpdateLifecycleToCompositingInputsClean();
    // Simulate that PaintInvalidator updates visual rect.
    box.GetMutableForPainting().SetVisualRect(
        IntRect(visual_rect.Location(), RoundedIntSize(box.Size())));

    EXPECT_EQ(PaintInvalidationReason::kGeometry,
              ComputePaintInvalidationReason(box, visual_rect, paint_offset));
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
  FragmentData fragment_data_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(BoxPaintInvalidatorTest);

TEST_P(BoxPaintInvalidatorTest, ComputePaintInvalidationReasonPaintingNothing) {
  SetUpHTML();
  auto& target = *GetDocument().getElementById("target");
  auto& box = *ToLayoutBox(target.GetLayoutObject());
  // Remove border.
  target.setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(box.PaintedOutputOfObjectHasNoEffectRegardlessOfSize());
  auto visual_rect = box.FirstFragment().VisualRect();

  // No geometry change.
  EXPECT_EQ(
      PaintInvalidationReason::kNone,
      ComputePaintInvalidationReason(box, visual_rect, visual_rect.Location()));

  // Paint offset change.
  EXPECT_EQ(PaintInvalidationReason::kNone,
            ComputePaintInvalidationReason(
                box, visual_rect, visual_rect.Location() + IntSize(10, 20)));

  // Visual rect size change.
  auto old_visual_rect = visual_rect;
  target.setAttribute(html_names::kStyleAttr, "width: 200px");
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  // Simulate that PaintInvalidator updates visual rect.
  box.GetMutableForPainting().SetVisualRect(
      IntRect(visual_rect.Location(), RoundedIntSize(box.Size())));

  EXPECT_EQ(PaintInvalidationReason::kNone,
            ComputePaintInvalidationReason(box, old_visual_rect,
                                           old_visual_rect.Location()));
}

TEST_P(BoxPaintInvalidatorTest, ComputePaintInvalidationReasonBasic) {
  SetUpHTML();
  auto& target = *GetDocument().getElementById("target");
  auto& box = *ToLayoutBox(target.GetLayoutObject());
  // Remove border.
  target.setAttribute(html_names::kClassAttr, "");
  target.setAttribute(html_names::kStyleAttr, "background: blue");
  UpdateAllLifecyclePhasesForTest();

  box.SetShouldCheckForPaintInvalidation();
  auto visual_rect = box.FirstFragment().VisualRect();
  EXPECT_EQ(IntRect(0, 0, 50, 100), visual_rect);

  // No geometry change.
  EXPECT_EQ(
      PaintInvalidationReason::kNone,
      ComputePaintInvalidationReason(box, visual_rect, visual_rect.Location()));

  // Visual rect size change.
  auto old_visual_rect = visual_rect;
  target.setAttribute(html_names::kStyleAttr, "background: blue; width: 200px");
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  // Simulate that PaintInvalidator updates visual rect.
  box.GetMutableForPainting().SetVisualRect(
      IntRect(visual_rect.Location(), RoundedIntSize(box.Size())));

  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            ComputePaintInvalidationReason(box, old_visual_rect,
                                           old_visual_rect.Location()));

  // Visual rect size change, with paint offset different from location of
  // visual rect.
  PhysicalOffset fake_paint_offset =
      PhysicalOffset(visual_rect.Location()) + PhysicalOffset(10, 20);
  box.GetMutableForPainting().FirstFragment().SetPaintOffset(fake_paint_offset);
  EXPECT_EQ(
      PaintInvalidationReason::kGeometry,
      ComputePaintInvalidationReason(box, old_visual_rect, fake_paint_offset));

  // Should use the existing full paint invalidation reason regardless of
  // geometry change.
  box.SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kStyle);
  EXPECT_EQ(
      PaintInvalidationReason::kStyle,
      ComputePaintInvalidationReason(box, visual_rect, visual_rect.Location()));
  EXPECT_EQ(PaintInvalidationReason::kStyle,
            ComputePaintInvalidationReason(
                box, visual_rect, visual_rect.Location() + IntSize(10, 20)));
}

TEST_P(BoxPaintInvalidatorTest, ComputePaintInvalidationReasonOtherCases) {
  SetUpHTML();
  auto& target = *GetDocument().getElementById("target");

  // The target initially has border.
  ExpectFullPaintInvalidationOnGeometryChange("With border");

  // Clear border, set background.
  target.setAttribute(html_names::kClassAttr, "background");
  target.setAttribute(html_names::kStyleAttr, "border-radius: 5px");
  ExpectFullPaintInvalidationOnGeometryChange("With border-radius");

  target.setAttribute(html_names::kStyleAttr, "-webkit-mask: url(#)");
  ExpectFullPaintInvalidationOnGeometryChange("With mask");

  target.setAttribute(html_names::kStyleAttr, "filter: blur(5px)");
  ExpectFullPaintInvalidationOnGeometryChange("With filter");

  target.setAttribute(html_names::kStyleAttr, "outline: 2px solid blue");
  ExpectFullPaintInvalidationOnGeometryChange("With outline");

  target.setAttribute(html_names::kStyleAttr, "box-shadow: inset 3px 2px");
  ExpectFullPaintInvalidationOnGeometryChange("With box-shadow");

  target.setAttribute(html_names::kStyleAttr,
                      "clip-path: circle(50% at 0 50%)");
  ExpectFullPaintInvalidationOnGeometryChange("With clip-path");
}

}  // namespace blink
