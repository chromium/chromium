// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"

#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class CompositedLayerMappingTest : public RenderingTest {
 public:
  CompositedLayerMappingTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()) {}

 protected:
  IntRect RecomputeInterestRect(const GraphicsLayer* graphics_layer) {
    return static_cast<CompositedLayerMapping&>(graphics_layer->Client())
        .RecomputeInterestRect(graphics_layer);
  }

  IntRect ComputeInterestRect(
      GraphicsLayer* graphics_layer,
      IntRect previous_interest_rect) {
    return static_cast<CompositedLayerMapping&>(graphics_layer->Client())
        .ComputeInterestRect(graphics_layer, previous_interest_rect);
  }

  bool ShouldFlattenTransform(const GraphicsLayer& layer) const {
    return layer.ShouldFlattenTransform();
  }

  bool InterestRectChangedEnoughToRepaint(const IntRect& previous_interest_rect,
                                          const IntRect& new_interest_rect,
                                          const IntSize& layer_size) {
    return CompositedLayerMapping::InterestRectChangedEnoughToRepaint(
        previous_interest_rect, new_interest_rect, layer_size);
  }

  IntRect PreviousInterestRect(const GraphicsLayer* graphics_layer) {
    return graphics_layer->previous_interest_rect_;
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }

  void TearDown() override { RenderingTest::TearDown(); }
};

// Tests the pre-BlinkGenPropertyTrees composited layer mapping code. With BGPT,
// some layer updates are skipped (see: CLM::UpdateGraphicsLayerConfiguration
// and CLM::UpdateStickyConstraints).
class CompositedLayerMappingTestWithoutBGPT
    : private ScopedBlinkGenPropertyTreesForTest,
      public CompositedLayerMappingTest {
 public:
  CompositedLayerMappingTestWithoutBGPT()
      : ScopedBlinkGenPropertyTreesForTest(false) {}
};

TEST_F(CompositedLayerMappingTest, SubpixelAccumulationChange) {
  SetBodyInnerHTML(
      "<div id='target' style='will-change: transform; background: lightblue; "
      "position: relative; left: 0.4px; width: 100px; height: 100px'>");

  Element* target = GetDocument().getElementById("target");
  target->SetInlineStyleProperty(CSSPropertyLeft, "0.6px");

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  // Directly composited layers are not invalidated on subpixel accumulation
  // change.
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking()
                   ->GetPaintController()
                   .GetPaintArtifact()
                   .IsEmpty());
}

TEST_F(CompositedLayerMappingTest,
       SubpixelAccumulationChangeUnderInvalidation) {
  ScopedPaintUnderInvalidationCheckingForTest test(true);
  SetBodyInnerHTML(
      "<div id='target' style='will-change: transform; background: lightblue; "
      "position: relative; left: 0.4px; width: 100px; height: 100px'>");

  Element* target = GetDocument().getElementById("target");
  target->SetInlineStyleProperty(CSSPropertyLeft, "0.6px");

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  // Directly composited layers are not invalidated on subpixel accumulation
  // change.
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()
                  ->GetPaintController()
                  .GetPaintArtifact()
                  .IsEmpty());
}

TEST_F(CompositedLayerMappingTest,
       SubpixelAccumulationChangeIndirectCompositing) {
  SetBodyInnerHTML(

      "<div id='target' style='background: lightblue; "
      "    position: relative; top: -10px; left: 0.4px; width: 100px;"
      "    height: 100px; transform: translateX(0)'>"
      "  <div style='position; relative; width: 100px; height: 100px;"
      "    background: lightgray; will-change: transform'></div>"
      "</div>");

  Element* target = GetDocument().getElementById("target");
  target->SetInlineStyleProperty(CSSPropertyLeft, "0.6px");

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  // The PaintArtifact should have been deleted because paint was
  // invalidated for subpixel accumulation change.
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()
                  ->GetPaintController()
                  .GetPaintArtifact()
                  .IsEmpty());
}

TEST_F(CompositedLayerMappingTest, SimpleInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px; will-change: "
      "transform'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GetCompositedLayerMapping());
  EXPECT_EQ(IntRect(0, 0, 200, 200),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, TallLayerInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  // Screen-space visible content rect is [8, 8, 200, 600]. Mapping back to
  // local, adding 4000px in all directions, then clipping, yields this rect.
  EXPECT_EQ(IntRect(0, 0, 200, 4592),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, TallCompositedScrolledLayerInterestRect) {
  SetBodyInnerHTML(R"HTML(
      <div style='width: 200px; height: 1000px;'></div>
      <div id='target'
           style='width: 200px; height: 10000px; will-change: transform'>
       </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 8000),
                                                          kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 2992, 200, 7008),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, TallNonCompositedScrolledLayerInterestRect) {
  SetHtmlInnerHTML(R"HTML(
    <div style='width: 200px; height: 11000px;'></div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 8000),
                                                          kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  PaintLayer* paint_layer = GetDocument().GetLayoutView()->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 4000, 800, 7016),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, TallLayerWholeDocumentInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform'></div>");

  GetDocument().GetSettings()->SetMainFrameClipsContent(false);

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GetCompositedLayerMapping());
  // Clipping is disabled in recomputeInterestRect.
  EXPECT_EQ(IntRect(0, 0, 200, 10000),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
  EXPECT_EQ(
      IntRect(0, 0, 200, 10000),
      ComputeInterestRect(paint_layer->GraphicsLayerBacking(), IntRect()));
}

TEST_F(CompositedLayerMappingTest, VerticalRightLeftWritingModeDocument) {
  SetBodyInnerHTML(R"HTML(
    <style>html,body { margin: 0px } html { -webkit-writing-mode:
    vertical-rl}</style> <div id='target' style='width: 10000px; height:
    200px;'></div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(-5000, 0), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  PaintLayer* paint_layer = GetDocument().GetLayoutView()->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GetCompositedLayerMapping());
  // A scroll by -5000px is equivalent to a scroll by (10000 - 5000 - 800)px =
  // 4200px in non-RTL mode. Expanding the resulting rect by 4000px in each
  // direction yields this result.
  EXPECT_EQ(IntRect(200, 0, 8800, 600),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, RotatedInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px; will-change: "
      "transform; transform: rotateZ(45deg)'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 200, 200),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, RotatedInterestRectNear90Degrees) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 10000px; height: 200px; will-change: "
      "transform; transform: rotateY(89.9999deg)'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  // Because the layer is rotated to almost 90 degrees, floating-point error
  // leads to a reverse-projected rect that is much much larger than the
  // original layer size in certain dimensions. In such cases, we often fall
  // back to the 4000px interest rect padding amount.
  EXPECT_EQ(IntRect(0, 0, 4000, 200),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, LargeScaleInterestRect) {
  // It's rotated 90 degrees about the X axis, which means its visual content
  // rect is empty, and so the interest rect is the default (0, 0, 4000, 4000)
  // intersected with the layer bounds.
  SetBodyInnerHTML(R"HTML(
    <style>
      .container {
        height: 1080px;
        width: 1920px;
        transform: scale(0.0859375);
        transform-origin: 0 0 0;
        background:blue;
      }
      .wrapper {
          height: 92px;
          width: 165px;
          overflow: hidden;
      }
      .posabs {
          position: absolute;
          width: 300px;
          height: 300px;
          top: 5000px;
      }
    </style>
    <div class='wrapper'>
      <div id='target' class='container'>
        <div class='posabs'></div>
        <div id='target' style='will-change: transform'
    class='posabs'></div>
      </div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 1920, 5300),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, PerspectiveInterestRect) {
  SetBodyInnerHTML(R"HTML(<div style='left: 400px; position: absolute;'>
    <div id=target style='transform: perspective(1000px) rotateX(-100deg);'>
      <div style='width: 1200px; height: 835px; background: lightblue;
          border: 1px solid black'></div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 1202, 837),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, 3D90DegRotatedTallInterestRect) {
  // It's rotated 90 degrees about the X axis, which means its visual content
  // rect is empty, and so the interest rect is the default (0, 0, 4000, 4000)
  // intersected with the layer bounds.
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform; transform: rotateY(90deg)'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 200, 4000),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, 3D45DegRotatedTallInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform; transform: rotateY(45deg)'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 200, 4592),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, RotatedTallInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform; transform: rotateZ(45deg)'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 200, 4000),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, WideLayerInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 10000px; height: 200px; will-change: "
      "transform'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  // Screen-space visible content rect is [8, 8, 800, 200] (the screen is
  // 800x600).  Mapping back to local, adding 4000px in all directions, then
  // clipping, yields this rect.
  EXPECT_EQ(IntRect(0, 0, 4792, 200),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, FixedPositionInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 300px; height: 400px; will-change: "
      "transform; position: fixed; top: 100px; left: 200px;'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 300, 400),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, LayerOffscreenInterestRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width: 200px; height: 200px; will-change:
    transform; position: absolute; top: 9000px; left: 0px;'>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  // Offscreen layers are painted as usual.
  EXPECT_EQ(IntRect(0, 0, 200, 200),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, ScrollingLayerInterestRect) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div::-webkit-scrollbar{ width: 5px; }
    </style>
    <div id='target' style='width: 200px; height: 200px; will-change:
    transform; overflow: scroll'>
    <div style='width: 100px; height: 10000px'></div></div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  // Offscreen layers are painted as usual.
  ASSERT_TRUE(
      paint_layer->GetCompositedLayerMapping()->ScrollingContentsLayer());
  // In screen space, the scroller is (8, 8, 195, 193) (because of overflow clip
  // of 'target', scrollbar and root margin).
  // Applying the viewport clip of the root has no effect because
  // the clip is already small. Mapping it down into the graphics layer
  // space yields (0, 0, 195, 193). This is then expanded by 4000px.
  EXPECT_EQ(IntRect(0, 0, 195, 4193),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, ClippedBigLayer) {
  SetBodyInnerHTML(R"HTML(
    <div style='width: 1px; height: 1px; overflow: hidden'>
    <div id='target' style='width: 10000px; height: 10000px; will-change:
    transform'></div></div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  // Offscreen layers are painted as usual.
  EXPECT_EQ(IntRect(0, 0, 4001, 4001),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTestWithoutBGPT, ClippingMaskLayer) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  const AtomicString style_without_clipping =
      "backface-visibility: hidden; width: 200px; height: 200px";
  const AtomicString style_with_border_radius =
      style_without_clipping + "; border-radius: 10px";
  const AtomicString style_with_clip_path =
      style_without_clipping + "; -webkit-clip-path: inset(10px)";

  SetBodyInnerHTML("<video id='video' src='x' style='" +
                   style_without_clipping + "'></video>");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* video_element = GetDocument().getElementById("video");
  GraphicsLayer* graphics_layer =
      ToLayoutBoxModelObject(video_element->GetLayoutObject())
          ->Layer()
          ->GraphicsLayerBacking();
  EXPECT_FALSE(graphics_layer->MaskLayer());
  EXPECT_FALSE(graphics_layer->ContentsClippingMaskLayer());

  video_element->setAttribute(HTMLNames::styleAttr, style_with_border_radius);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(graphics_layer->MaskLayer());
  EXPECT_TRUE(graphics_layer->ContentsClippingMaskLayer());

  video_element->setAttribute(HTMLNames::styleAttr, style_with_clip_path);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(graphics_layer->MaskLayer());
  EXPECT_FALSE(graphics_layer->ContentsClippingMaskLayer());

  video_element->setAttribute(HTMLNames::styleAttr, style_without_clipping);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(graphics_layer->MaskLayer());
  EXPECT_FALSE(graphics_layer->ContentsClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTest, ScrollContentsFlattenForScroller) {
  SetBodyInnerHTML(R"HTML(
    <style>div::-webkit-scrollbar{ width: 5px; }</style>
    <div id='scroller' style='width: 100px; height: 100px; overflow:
    scroll; will-change: transform'>
    <div style='width: 1000px; height: 1000px;'>Foo</div>Foo</div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* element = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  CompositedLayerMapping* composited_layer_mapping =
      paint_layer->GetCompositedLayerMapping();

  ASSERT_TRUE(composited_layer_mapping);

  EXPECT_FALSE(
      ShouldFlattenTransform(*composited_layer_mapping->MainGraphicsLayer()));
  EXPECT_FALSE(
      ShouldFlattenTransform(*composited_layer_mapping->ScrollingLayer()));
  EXPECT_TRUE(ShouldFlattenTransform(
      *composited_layer_mapping->ScrollingContentsLayer()));
}

TEST_F(CompositedLayerMappingTest, InterestRectChangedEnoughToRepaintEmpty) {
  IntSize layer_size(1000, 1000);
  // Both empty means there is nothing to do.
  EXPECT_FALSE(
      InterestRectChangedEnoughToRepaint(IntRect(), IntRect(), layer_size));
  // Going from empty to non-empty means we must re-record because it could be
  // the first frame after construction or Clear.
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(IntRect(), IntRect(0, 0, 1, 1),
                                                 layer_size));
  // Going from non-empty to empty is not special-cased.
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(IntRect(0, 0, 1, 1),
                                                  IntRect(), layer_size));
}

TEST_F(CompositedLayerMappingTest,
       InterestRectChangedEnoughToRepaintNotBigEnough) {
  IntSize layer_size(1000, 1000);
  IntRect previous_interest_rect(100, 100, 100, 100);
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(100, 100, 90, 90), layer_size));
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(100, 100, 100, 100), layer_size));
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(1, 1, 200, 200), layer_size));
}

TEST_F(CompositedLayerMappingTest,
       InterestRectChangedEnoughToRepaintNotBigEnoughButNewAreaTouchesEdge) {
  IntSize layer_size(500, 500);
  IntRect previous_interest_rect(100, 100, 100, 100);
  // Top edge.
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(100, 0, 100, 200), layer_size));
  // Left edge.
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(0, 100, 200, 100), layer_size));
  // Bottom edge.
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(100, 100, 100, 400), layer_size));
  // Right edge.
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, IntRect(100, 100, 400, 100), layer_size));
}

// Verifies that having a current viewport that touches a layer edge does not
// force re-recording.
TEST_F(CompositedLayerMappingTest,
       InterestRectChangedEnoughToRepaintCurrentViewportTouchesEdge) {
  IntSize layer_size(500, 500);
  IntRect new_interest_rect(100, 100, 300, 300);
  // Top edge.
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      IntRect(100, 0, 100, 100), new_interest_rect, layer_size));
  // Left edge.
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      IntRect(0, 100, 100, 100), new_interest_rect, layer_size));
  // Bottom edge.
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      IntRect(300, 400, 100, 100), new_interest_rect, layer_size));
  // Right edge.
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      IntRect(400, 300, 100, 100), new_interest_rect, layer_size));
}

TEST_F(CompositedLayerMappingTest,
       InterestRectChangedEnoughToRepaintScrollScenarios) {
  IntSize layer_size(1000, 1000);
  IntRect previous_interest_rect(100, 100, 100, 100);
  IntRect new_interest_rect(previous_interest_rect);
  new_interest_rect.Move(512, 0);
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, new_interest_rect, layer_size));
  new_interest_rect.Move(0, 512);
  EXPECT_FALSE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, new_interest_rect, layer_size));
  new_interest_rect.Move(1, 0);
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, new_interest_rect, layer_size));
  new_interest_rect.Move(-1, 1);
  EXPECT_TRUE(InterestRectChangedEnoughToRepaint(
      previous_interest_rect, new_interest_rect, layer_size));
}

TEST_F(CompositedLayerMappingTest, InterestRectChangeOnViewportScroll) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { width: 0; height: 0; }
      body { margin: 0; }
    </style>
    <div id='div' style='width: 100px; height: 10000px'>Text</div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  GraphicsLayer* root_scrolling_layer =
      GetDocument().GetLayoutView()->Layer()->GraphicsLayerBacking();
  EXPECT_EQ(IntRect(0, 0, 800, 4600),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 300),
                                                          kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Still use the previous interest rect because the recomputed rect hasn't
  // changed enough.
  EXPECT_EQ(IntRect(0, 0, 800, 4900),
            RecomputeInterestRect(root_scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 800, 4600),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 600),
                                                          kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Use recomputed interest rect because it changed enough.
  EXPECT_EQ(IntRect(0, 0, 800, 5200),
            RecomputeInterestRect(root_scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 800, 5200),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 5400),
                                                          kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(IntRect(0, 1400, 800, 8600),
            RecomputeInterestRect(root_scrolling_layer));
  EXPECT_EQ(IntRect(0, 1400, 800, 8600),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 9000),
                                                          kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Still use the previous interest rect because it contains the recomputed
  // interest rect.
  EXPECT_EQ(IntRect(0, 5000, 800, 5000),
            RecomputeInterestRect(root_scrolling_layer));
  EXPECT_EQ(IntRect(0, 1400, 800, 8600),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 2000),
                                                          kProgrammaticScroll);
  // Use recomputed interest rect because it changed enough.
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(IntRect(0, 0, 800, 6600),
            RecomputeInterestRect(root_scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 800, 6600),
            PreviousInterestRect(root_scrolling_layer));
}

TEST_F(CompositedLayerMappingTest, InterestRectChangeOnShrunkenViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { width: 0; height: 0; }
      body { margin: 0; }
    </style>
    <div id='div' style='width: 100px; height: 10000px'>Text</div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  GraphicsLayer* root_scrolling_layer =
      GetDocument().GetLayoutView()->Layer()->GraphicsLayerBacking();
  EXPECT_EQ(IntRect(0, 0, 800, 4600),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->SetFrameRect(IntRect(0, 0, 800, 60));
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Repaint required, so interest rect should be updated to shrunken size.
  EXPECT_EQ(IntRect(0, 0, 800, 4060),
            RecomputeInterestRect(root_scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 800, 4060),
            PreviousInterestRect(root_scrolling_layer));
}

TEST_F(CompositedLayerMappingTest, InterestRectChangeOnScroll) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);

  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { width: 0; height: 0; }
      body { margin: 0; }
    </style>
    <div id='scroller' style='width: 400px; height: 400px; overflow:
    scroll'>
      <div id='content' style='width: 100px; height: 10000px'>Text</div>
    </div
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* scroller = GetDocument().getElementById("scroller");
  GraphicsLayer* scrolling_layer =
      scroller->GetLayoutBox()->Layer()->GraphicsLayerBacking();
  EXPECT_EQ(IntRect(0, 0, 400, 4400), PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(300);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Still use the previous interest rect because the recomputed rect hasn't
  // changed enough.
  EXPECT_EQ(IntRect(0, 0, 400, 4700), RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 400, 4400), PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(600);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Use recomputed interest rect because it changed enough.
  EXPECT_EQ(IntRect(0, 0, 400, 5000), RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 400, 5000), PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(5600);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(IntRect(0, 1600, 400, 8400),
            RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 1600, 400, 8400), PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(9000);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Still use the previous interest rect because it contains the recomputed
  // interest rect.
  EXPECT_EQ(IntRect(0, 5000, 400, 5000),
            RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 1600, 400, 8400), PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(2000);
  // Use recomputed interest rect because it changed enough.
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(IntRect(0, 0, 400, 6400), RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 400, 6400), PreviousInterestRect(scrolling_layer));
}

TEST_F(CompositedLayerMappingTest,
       InterestRectShouldChangeOnPaintInvalidation) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);

  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { width: 0; height: 0; }
      body { margin: 0; }
    </style>
    <div id='scroller' style='width: 400px; height: 400px; overflow:
    scroll'>
      <div id='content' style='width: 100px; height: 10000px'>Text</div>
    </div
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* scroller = GetDocument().getElementById("scroller");
  GraphicsLayer* scrolling_layer =
      scroller->GetLayoutBox()->Layer()->GraphicsLayerBacking();

  scroller->setScrollTop(5400);
  GetDocument().View()->UpdateAllLifecyclePhases();
  scroller->setScrollTop(9400);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(IntRect(0, 5400, 400, 4600),
            RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 5400, 400, 4600), PreviousInterestRect(scrolling_layer));

  // Paint invalidation and repaint should change previous paint interest rect.
  GetDocument().getElementById("content")->setTextContent("Change");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(IntRect(0, 5400, 400, 4600),
            RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 5400, 400, 4600), PreviousInterestRect(scrolling_layer));
}

TEST_F(CompositedLayerMappingTest,
       InterestRectOfSquashingLayerWithNegativeOverflow) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; font-size: 16px; }</style>
    <div style='position: absolute; top: -500px; width: 200px; height:
    700px; will-change: transform'></div>
    <div id='squashed' style='position: absolute; top: 190px;'>
      <div id='inside' style='width: 100px; height: 100px; text-indent:
    -10000px'>text</div>
    </div>
  )HTML");

  EXPECT_EQ(GetDocument()
                .getElementById("inside")
                ->GetLayoutBox()
                ->VisualOverflowRect()
                .Size()
                .Height(),
            100);

  CompositedLayerMapping* grouped_mapping = GetDocument()
                                                .getElementById("squashed")
                                                ->GetLayoutBox()
                                                ->Layer()
                                                ->GroupedMapping();
  // The squashing layer is at (-10000, 190, 10100, 100) in viewport
  // coordinates.
  // The following rect is at (-4000, 190, 4100, 100) in viewport coordinates.
  EXPECT_EQ(IntRect(6000, 0, 4100, 100),
            grouped_mapping->ComputeInterestRect(
                grouped_mapping->SquashingLayer(), IntRect()));
}

TEST_F(CompositedLayerMappingTest,
       InterestRectOfSquashingLayerWithAncestorClip) {
  SetBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div style='overflow: hidden; width: 400px; height: 400px'>"
      "  <div style='position: relative; backface-visibility: hidden'>"
      "    <div style='position: absolute; top: -500px; width: 200px; height: "
      "700px; backface-visibility: hidden'></div>"
      // Above overflow:hidden div and two composited layers make the squashing
      // layer a child of an ancestor clipping layer.
      "    <div id='squashed' style='height: 1000px; width: 10000px; right: 0; "
      "position: absolute'></div>"
      "  </div>"
      "</div>");

  CompositedLayerMapping* grouped_mapping = GetDocument()
                                                .getElementById("squashed")
                                                ->GetLayoutBox()
                                                ->Layer()
                                                ->GroupedMapping();
  // The squashing layer is at (-9600, 0, 10000, 1000) in viewport coordinates.
  // The following rect is at (-4000, 0, 4400, 1000) in viewport coordinates.
  EXPECT_EQ(IntRect(5600, 0, 4400, 1000),
            grouped_mapping->ComputeInterestRect(
                grouped_mapping->SquashingLayer(), IntRect()));
}

TEST_F(CompositedLayerMappingTest, InterestRectOfIframeInScrolledDiv) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div style='width: 200; height: 8000px'></div>
    <iframe src='http://test.com' width='500' height='500'
    frameBorder='0'>
    </iframe>
  )HTML");
  SetChildFrameHTML(
      "<style>body { margin: 0; } #target { width: 200px; height: 200px; "
      "will-change: transform}</style><div id=target></div>");

  // Scroll 8000 pixels down to move the iframe into view.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 8000.0), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* target = ChildDocument().getElementById("target");
  ASSERT_TRUE(target);

  EXPECT_EQ(
      IntRect(0, 0, 200, 200),
      RecomputeInterestRect(
          target->GetLayoutObject()->EnclosingLayer()->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, InterestRectOfScrolledIframe) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; } ::-webkit-scrollbar { display: none;
    }</style>
    <iframe src='http://test.com' width='500' height='500'
    frameBorder='0'>
    </iframe>
  )HTML");
  SetChildFrameHTML(
      "<style>body { margin: 0; } #target { width: 200px; "
      "height: 8000px;}</style><div id=target></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  // Scroll 7500 pixels down to bring the scrollable area to the bottom.
  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 7500.0), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(ChildDocument().View()->GetLayoutView()->HasLayer());
  EXPECT_EQ(IntRect(0, 3500, 500, 4500),
            RecomputeInterestRect(ChildDocument()
                                      .View()
                                      ->GetLayoutView()
                                      ->EnclosingLayer()
                                      ->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, InterestRectOfIframeWithContentBoxOffset) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  // Set a 10px border in order to have a contentBoxOffset for the iframe
  // element.
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; } #frame { border: 10px solid black; }
    ::-webkit-scrollbar { display: none; }</style>
    <iframe src='http://test.com' width='500' height='500'
    frameBorder='0'>
    </iframe>
  )HTML");
  SetChildFrameHTML(
      "<style>body { margin: 0; } #target { width: 200px; "
      "height: 8000px;}</style> <div id=target></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  // Scroll 3000 pixels down to bring the scrollable area to somewhere in the
  // middle.
  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 3000.0), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(ChildDocument().View()->GetLayoutView()->HasLayer());
  EXPECT_EQ(IntRect(0, 0, 500, 7500),
            RecomputeInterestRect(ChildDocument()
                                      .View()
                                      ->GetLayoutView()
                                      ->EnclosingLayer()
                                      ->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, InterestRectOfIframeWithFixedContents) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(R"HTML(
    <style> * { margin:0; } </style>
    <iframe src='http://test.com' width='500' height='500' frameBorder='0'>
    </iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>body { margin:0; } ::-webkit-scrollbar { display:none; }</style>
    <div id='forcescroll' style='height:6000px;'></div>
    <div id='fixed' style='
        position:fixed; top:0; left:0; width:400px; height:300px;'>
      <div id='leftbox' style='
          position:absolute; left:-5000px; width:10px; height:10px;'></div>
      <div id='child' style='
          position:absolute; top:0; left:0; width:400px; height:300px;'></div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  auto* fixed = ChildDocument().getElementById("fixed")->GetLayoutObject();
  auto* graphics_layer = fixed->EnclosingLayer()->GraphicsLayerBacking(fixed);

  // The graphics layer has dimensions 5400x300 but the interest rect clamps
  // this to the right-most 4000x4000 area.
  EXPECT_EQ(IntRect(1000, 0, 4400, 300), RecomputeInterestRect(graphics_layer));

  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 3000.0), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Because the fixed element does not scroll, the interest rect is unchanged.
  EXPECT_EQ(IntRect(1000, 0, 4400, 300), RecomputeInterestRect(graphics_layer));
}

TEST_F(CompositedLayerMappingTest, ScrolledFixedPositionInterestRect) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(R"HTML(
    <style>body { margin:0; } ::-webkit-scrollbar { display:none; }</style>
    <div id="fixed" style="position: fixed;">
      <div style="background: blue; width: 30px; height: 30px;"></div>
      <div style="position: absolute; transform: translateY(-4500px);
          top: 0; left: 0; width: 100px; height: 100px;"></div>
    </div>
    <div id="forcescroll" style="height: 2000px;"></div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  auto* fixed = GetDocument().getElementById("fixed")->GetLayoutObject();
  auto* graphics_layer = fixed->EnclosingLayer()->GraphicsLayerBacking(fixed);
  EXPECT_EQ(IntRect(0, 500, 100, 4030), RecomputeInterestRect(graphics_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 200.0), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Because the fixed element does not scroll, the interest rect is unchanged.
  EXPECT_EQ(IntRect(0, 500, 100, 4030), RecomputeInterestRect(graphics_layer));
}

TEST_F(CompositedLayerMappingTest,
       ScrollingContentsAndForegroundLayerPaintingPhase) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position: relative; z-index: 1; overflow:
    scroll; width: 300px; height: 300px'>
        <div id='negative-composited-child' style='background-color: red;
    width: 1px; height: 1px; position: absolute; backface-visibility:
    hidden; z-index: -1'></div>
        <div style='background-color: blue; width: 2000px; height: 2000px;
    position: relative; top: 10px'></div>
    </div>
  )HTML");

  CompositedLayerMapping* mapping =
      ToLayoutBlock(GetLayoutObjectByElementId("container"))
          ->Layer()
          ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping->ScrollingContentsLayer());
  EXPECT_EQ(static_cast<GraphicsLayerPaintingPhase>(
                kGraphicsLayerPaintOverflowContents |
                kGraphicsLayerPaintCompositedScroll),
            mapping->ScrollingContentsLayer()->PaintingPhase());
  ASSERT_TRUE(mapping->ForegroundLayer());
  EXPECT_EQ(
      static_cast<GraphicsLayerPaintingPhase>(
          kGraphicsLayerPaintForeground | kGraphicsLayerPaintOverflowContents),
      mapping->ForegroundLayer()->PaintingPhase());
  // Regression test for crbug.com/767908: a foreground layer should also
  // participates hit testing.
  EXPECT_TRUE(mapping->ForegroundLayer()->GetHitTestableWithoutDrawsContent());

  Element* negative_composited_child =
      GetDocument().getElementById("negative-composited-child");
  negative_composited_child->parentNode()->RemoveChild(
      negative_composited_child);
  GetDocument().View()->UpdateAllLifecyclePhases();

  mapping = ToLayoutBlock(GetLayoutObjectByElementId("container"))
                ->Layer()
                ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping->ScrollingContentsLayer());
  EXPECT_EQ(
      static_cast<GraphicsLayerPaintingPhase>(
          kGraphicsLayerPaintOverflowContents |
          kGraphicsLayerPaintCompositedScroll | kGraphicsLayerPaintForeground),
      mapping->ScrollingContentsLayer()->PaintingPhase());
  EXPECT_FALSE(mapping->ForegroundLayer());
}

TEST_F(CompositedLayerMappingTest,
       DecorationOutlineLayerOnlyCreatedInCompositedScrolling) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #target { overflow: scroll; height: 200px; width: 200px; will-change:
    transform; background: white local content-box;
    outline: 1px solid blue; outline-offset: -2px;}
    #scrolled { height: 300px; }
    </style>
    <div id="parent">
      <div id="target"><div id="scrolled"></div></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  // Decoration outline layer is created when composited scrolling.
  EXPECT_TRUE(paint_layer->HasCompositedLayerMapping());
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());

  CompositedLayerMapping* mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_TRUE(mapping->DecorationOutlineLayer());

  // No decoration outline layer is created when not composited scrolling.
  element->setAttribute(HTMLNames::styleAttr, "overflow: visible;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(mapping->DecorationOutlineLayer());
}

TEST_F(CompositedLayerMappingTest,
       DecorationOutlineLayerCreatedAndDestroyedInCompositedScrolling) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px; background:
    white local content-box; outline: 1px solid blue; contain: paint; }
    #scrolled { height: 300px; }
    </style>
    <div id="parent">
      <div id="scroller"><div id="scrolled"></div></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  CompositedLayerMapping* mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->DecorationOutlineLayer());

  // The decoration outline layer is created when composited scrolling
  // with an outline drawn over the composited scrolling region.
  scroller->setAttribute(HTMLNames::styleAttr, "outline-offset: -2px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(mapping->DecorationOutlineLayer());

  // The decoration outline layer is destroyed when the scrolling region
  // will not be covered up by the outline.
  scroller->removeAttribute(HTMLNames::styleAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->DecorationOutlineLayer());
}

TEST_F(CompositedLayerMappingTest,
       BackgroundPaintedIntoGraphicsLayerIfNotCompositedScrolling) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='overflow: scroll; width: 300px; height:
        300px; background: white; will-change: transform;'>
      <div style='background-color: blue; width: 2000px; height: 2000px;
           clip-path: circle(600px at 1000px 1000px);'></div>
    </div>
  )HTML");

  const auto* container = ToLayoutBox(GetLayoutObjectByElementId("container"));
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            container->GetBackgroundPaintLocation());

  // We currently don't use composited scrolling when the container has a
  // border-radius so even though we can paint the background onto the scrolling
  // contents layer we don't have a scrolling contents layer to paint into in
  // this case.
  const auto* mapping = container->Layer()->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->HasScrollingLayer());
  EXPECT_FALSE(mapping->BackgroundPaintsOntoScrollingContentsLayer());
}

TEST_F(CompositedLayerMappingTest,
       ScrollingLayerWithPerspectivePositionedCorrectly) {
  // Test positioning of a scrolling layer within an offset parent, both with
  // and without perspective.
  //
  // When a box shadow is used, the main graphics layer position is offset by
  // the shadow. The scrolling contents then need to be offset in the other
  // direction to compensate.  To make this a little clearer, for the first
  // example here the layer positions are calculated as:
  //
  //   graphics_layer_ x = left_pos - shadow_spread + shadow_x_offset
  //                     = 50 - 10 - 10
  //                     = 30
  //
  //   graphics_layer_ y = top_pos - shadow_spread + shadow_y_offset
  //                     = 50 - 10 + 0
  //                     = 40
  //
  //   contents x = 50 - graphics_layer_ x = 50 - 30 = 20
  //   contents y = 50 - graphics_layer_ y = 50 - 40 = 10
  //
  // The reason that perspective matters is that it affects which 'contents'
  // layer is offset; child_transform_layer_ when using perspective, or
  // scrolling_layer_ when there is no perspective.

  SetBodyInnerHTML(R"HTML(
    <div id='scroller' style='position: absolute; top: 50px; left: 50px;
        width: 400px; height: 245px; overflow: auto; will-change: transform;
        box-shadow: -10px 0 0 10px; perspective: 1px;'>
      <div style='position: absolute; top: 50px; bottom: 0; width: 200px;
          height: 200px;'></div>
      </div>
    <div id='scroller2' style='position: absolute; top: 400px; left: 50px;
        width: 400px; height: 245px; overflow: auto; will-change: transform;
        box-shadow: -10px 0 0 10px;'>
      <div style='position: absolute; top: 50px; bottom: 0; width: 200px;
          height: 200px;'></div>
    </div>
  )HTML");

  CompositedLayerMapping* mapping =
      ToLayoutBlock(GetLayoutObjectByElementId("scroller"))
          ->Layer()
          ->GetCompositedLayerMapping();

  CompositedLayerMapping* mapping2 =
      ToLayoutBlock(GetLayoutObjectByElementId("scroller2"))
          ->Layer()
          ->GetCompositedLayerMapping();

  ASSERT_TRUE(mapping);
  ASSERT_TRUE(mapping2);

  // The perspective scroller should have a child transform containing the
  // positional offset, and a scrolling layer that has no offset.

  GraphicsLayer* scrolling_layer = mapping->ScrollingLayer();
  GraphicsLayer* child_transform_layer = mapping->ChildTransformLayer();
  GraphicsLayer* main_graphics_layer = mapping->MainGraphicsLayer();

  ASSERT_TRUE(scrolling_layer);
  ASSERT_TRUE(child_transform_layer);

  EXPECT_FLOAT_EQ(30, main_graphics_layer->GetPosition().x());
  EXPECT_FLOAT_EQ(40, main_graphics_layer->GetPosition().y());
  EXPECT_FLOAT_EQ(0, scrolling_layer->GetPosition().x());
  EXPECT_FLOAT_EQ(0, scrolling_layer->GetPosition().y());
  EXPECT_FLOAT_EQ(20, child_transform_layer->GetPosition().x());
  EXPECT_FLOAT_EQ(10, child_transform_layer->GetPosition().y());

  // The non-perspective scroller should have no child transform and the
  // offset on the scroller layer directly.

  GraphicsLayer* scrolling_layer2 = mapping2->ScrollingLayer();
  GraphicsLayer* main_graphics_layer2 = mapping2->MainGraphicsLayer();

  ASSERT_TRUE(scrolling_layer2);
  ASSERT_FALSE(mapping2->ChildTransformLayer());

  EXPECT_FLOAT_EQ(30, main_graphics_layer2->GetPosition().x());
  EXPECT_FLOAT_EQ(390, main_graphics_layer2->GetPosition().y());
  EXPECT_FLOAT_EQ(20, scrolling_layer2->GetPosition().x());
  EXPECT_FLOAT_EQ(10, scrolling_layer2->GetPosition().y());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClippingMaskLayerUpdates) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #ancestor { width: 100px; height: 100px; overflow: hidden; }
      #child { width: 120px; height: 120px; background-color: green; }
    </style>
    <div id='ancestor'>
      <div id='child'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_FALSE(child_paint_layer);

  // Making the child conposited causes creation of an AncestorClippingLayer.
  child->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());

  // Adding border radius to the ancestor requires an
  // ancestorClippingMaskLayer for the child
  ancestor->setAttribute(HTMLNames::styleAttr, "border-radius: 40px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingMaskLayer());

  // Removing the border radius should remove the ancestorClippingMaskLayer
  // for the child
  ancestor->setAttribute(HTMLNames::styleAttr, "border-radius: 0px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());

  // Add border radius back so we can test one more case
  ancestor->setAttribute(HTMLNames::styleAttr, "border-radius: 40px;");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Now change the overflow to remove the need for an ancestor clip
  // on the child
  ancestor->setAttribute(HTMLNames::styleAttr, "overflow: visible");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_FALSE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClippingMaskLayerSiblingUpdates) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #ancestor { width: 200px; height: 200px; overflow: hidden; }
      #child1 { width: 10px;; height: 260px; position: relative;
                left: 0px; top: -30px; background-color: green; }
      #child2 { width: 10px;; height: 260px; position: relative;
                left: 190px; top: -260px; background-color: green; }
    </style>
    <div id='ancestor'>
      <div id='child1'></div>
      <div id='child2'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child1 = GetDocument().getElementById("child1");
  ASSERT_TRUE(child1);
  PaintLayer* child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  CompositedLayerMapping* child1_mapping =
      child1_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child1_mapping);

  Element* child2 = GetDocument().getElementById("child2");
  ASSERT_TRUE(child2);
  PaintLayer* child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  CompositedLayerMapping* child2_mapping =
      child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child2_mapping);

  // Making child1 composited causes creation of an AncestorClippingLayer.
  child1->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  child1_mapping = child1_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child1_mapping);
  EXPECT_TRUE(child1_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child1_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child1_mapping->AncestorClippingMaskLayer());
  child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  child2_mapping = child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child2_mapping);

  // Adding border radius to the ancestor requires an
  // ancestorClippingMaskLayer for child1
  ancestor->setAttribute(HTMLNames::styleAttr, "border-radius: 40px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  child1_mapping = child1_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child1_mapping);
  EXPECT_TRUE(child1_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child1_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child1_mapping->AncestorClippingMaskLayer());
  child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  child2_mapping = child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child2_mapping);

  // Making child2 composited causes creation of an AncestorClippingLayer
  // and a mask layer.
  child2->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  child1_mapping = child1_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child1_mapping);
  ASSERT_TRUE(child1_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child1_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child1_mapping->AncestorClippingMaskLayer());
  child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  child2_mapping = child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child2_mapping);
  ASSERT_TRUE(child2_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child2_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child2_mapping->AncestorClippingMaskLayer());

  // Removing will-change: transform on child1 should result in the removal
  // of all clipping and masking layers
  child1->setAttribute(HTMLNames::styleAttr, "will-change: none");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  child1_mapping = child1_paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(child1_mapping);
  child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  child2_mapping = child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child2_mapping);
  ASSERT_TRUE(child2_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child2_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child2_mapping->AncestorClippingMaskLayer());

  // Now change the overflow to remove the need for an ancestor clip
  // on the children
  ancestor->setAttribute(HTMLNames::styleAttr, "overflow: visible");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child1_paint_layer =
      ToLayoutBoxModelObject(child1->GetLayoutObject())->Layer();
  ASSERT_TRUE(child1_paint_layer);
  child1_mapping = child1_paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(child1_mapping);
  child2_paint_layer =
      ToLayoutBoxModelObject(child2->GetLayoutObject())->Layer();
  ASSERT_TRUE(child2_paint_layer);
  child2_mapping = child2_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child2_mapping);
  EXPECT_FALSE(child2_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child2_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClippingMaskLayerGrandchildUpdates) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #ancestor { width: 200px; height: 200px; overflow: hidden; }
      #child { width: 10px;; height: 260px; position: relative;
               left: 0px; top: -30px; background-color: green; }
      #grandchild { width: 10px;; height: 260px; position: relative;
                    left: 190px; top: -30px; background-color: green; }
    </style>
    <div id='ancestor'>
      <div id='child'>
        <div id='grandchild'></div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child_mapping);

  Element* grandchild = GetDocument().getElementById("grandchild");
  ASSERT_TRUE(grandchild);
  PaintLayer* grandchild_paint_layer =
      ToLayoutBoxModelObject(grandchild->GetLayoutObject())->Layer();
  ASSERT_TRUE(grandchild_paint_layer);
  CompositedLayerMapping* grandchild_mapping =
      grandchild_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(grandchild_mapping);

  // Making grandchild composited causes creation of an AncestorClippingLayer.
  grandchild->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child_mapping);
  grandchild_paint_layer =
      ToLayoutBoxModelObject(grandchild->GetLayoutObject())->Layer();
  ASSERT_TRUE(grandchild_paint_layer);
  grandchild_mapping = grandchild_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(grandchild_mapping);
  EXPECT_TRUE(grandchild_mapping->AncestorClippingLayer());
  EXPECT_FALSE(grandchild_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(grandchild_mapping->AncestorClippingMaskLayer());

  // Adding border radius to the ancestor requires an
  // ancestorClippingMaskLayer for grandchild
  ancestor->setAttribute(HTMLNames::styleAttr, "border-radius: 40px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child_mapping);
  grandchild_paint_layer =
      ToLayoutBoxModelObject(grandchild->GetLayoutObject())->Layer();
  ASSERT_TRUE(grandchild_paint_layer);
  grandchild_mapping = grandchild_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(grandchild_mapping);
  ASSERT_TRUE(grandchild_mapping->AncestorClippingLayer());
  EXPECT_TRUE(grandchild_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(grandchild_mapping->AncestorClippingMaskLayer());

  // Moving the grandchild out of the clip region should result in removal
  // of the mask layer. It also removes the grandchild from its own mapping
  // because it is now squashed.
  grandchild->setAttribute(HTMLNames::styleAttr,
                           "left: 250px; will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child_mapping);
  grandchild_paint_layer =
      ToLayoutBoxModelObject(grandchild->GetLayoutObject())->Layer();
  ASSERT_TRUE(grandchild_paint_layer);
  grandchild_mapping = grandchild_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(grandchild_mapping);
  ASSERT_TRUE(grandchild_mapping->AncestorClippingLayer());
  EXPECT_FALSE(grandchild_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(grandchild_mapping->AncestorClippingMaskLayer());

  // Now change the overflow to remove the need for an ancestor clip
  // on the children
  ancestor->setAttribute(HTMLNames::styleAttr, "overflow: visible");
  GetDocument().View()->UpdateAllLifecyclePhases();
  child_paint_layer = ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  child_mapping = child_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(child_mapping);
  grandchild_paint_layer =
      ToLayoutBoxModelObject(grandchild->GetLayoutObject())->Layer();
  ASSERT_TRUE(grandchild_paint_layer);
  grandchild_mapping = grandchild_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(grandchild_mapping);
  EXPECT_FALSE(grandchild_mapping->AncestorClippingLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskRequiredByBorderRadius) {
  // Verify that we create the mask layer when the child is contained within
  // the rectangular clip but not contained within the rounded rect clip.
  SetBodyInnerHTML(R"HTML(
    <style>
      #ancestor {
        width: 100px; height: 100px; overflow: hidden; border-radius: 20px;
      }
      #child { position: relative; left: 2px; top: 2px; width: 96px;
               height: 96px; background-color: green;
               will-change: transform;
      }
    </style>
    <div id='ancestor'>
      <div id='child'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskNotRequiredByNestedBorderRadius) {
  // This case has the child within all ancestors and does not require a
  // mask.
  SetBodyInnerHTML(R"HTML(
    <style>
      #grandparent {
        width: 200px; height: 200px; overflow: hidden; border-radius: 25px;
      }
      #parent { position: relative; left: 40px; top: 40px; width: 120px;
               height: 120px; border-radius: 10px; overflow: hidden;
      }
      #child { position: relative; left: 10px; top: 10px; width: 100px;
               height: 100px; background-color: green;
               will-change: transform;
      }
    </style>
    <div id='grandparent'>
      <div id='parent'>
        <div id='child'></div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskRequiredByParentBorderRadius) {
  // This case has the child within the grandparent but not the parent, and does
  // require a mask so that the parent will clip the corners.
  SetBodyInnerHTML(R"HTML(
    <style>
      #grandparent {
        width: 200px; height: 200px; overflow: hidden; border-radius: 25px;
      }
      #parent { position: relative; left: 40px; top: 40px; width: 120px;
               height: 120px; border-radius: 10px; overflow: hidden;
      }
      #child { position: relative; left: 1px; top: 1px; width: 118px;
               height: 118px; background-color: green;
               will-change: transform;
      }
    </style>
    <div id='grandparent'>
      <div id='parent'>
        <div id='child'></div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  ASSERT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  ASSERT_TRUE(child_mapping->AncestorClippingMaskLayer());
  auto layer_size = child_mapping->AncestorClippingMaskLayer()->Size();
  EXPECT_EQ(120, layer_size.width());
  EXPECT_EQ(120, layer_size.height());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskNotRequiredByParentBorderRadius) {
  // This case has the child within the grandparent but not the parent, and does
  // not require a mask because the parent does not have border radius
  SetBodyInnerHTML(R"HTML(
    <style>
      #grandparent {
        width: 200px; height: 200px; overflow: hidden; border-radius: 25px;
      }
      #parent { position: relative; left: 40px; top: 40px; width: 120px;
               height: 120px; overflow: hidden;
      }
      #child { position: relative; left: -10px; top: -10px; width: 140px;
               height: 140px; background-color: green;
               will-change: transform;
      }
    </style>
    <div id='grandparent'>
      <div id='parent'>
        <div id='child'></div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskRequiredByGrandparentBorderRadius1) {
  // This case has the child clipped by the grandparent border radius but not
  // the parent, and requires a mask to clip to the grandparent. Although in
  // an optimized world we would not need this because the parent clips out
  // the child before it is clipped by the grandparent.
  SetBodyInnerHTML(R"HTML(
    <style>
      #grandparent {
        width: 200px; height: 200px; overflow: hidden; border-radius: 25px;
      }
      #parent { position: relative; left: 40px; top: 40px; width: 120px;
               height: 120px; overflow: hidden;
      }
      #child { position: relative; left: -10px; top: -10px; width: 180px;
               height: 180px; background-color: green;
               will-change: transform;
      }
    </style>
    <div id='grandparent'>
      <div id='parent'>
        <div id='child'></div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  ASSERT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  ASSERT_TRUE(child_mapping->AncestorClippingMaskLayer());
  auto layer_size = child_mapping->AncestorClippingMaskLayer()->Size();
  EXPECT_EQ(120, layer_size.width());
  EXPECT_EQ(120, layer_size.height());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskRequiredByGrandparentBorderRadius2) {
  // Similar to the previous case, but here we really do need the mask.
  SetBodyInnerHTML(R"HTML(
    <style>
      #grandparent {
        width: 200px; height: 200px; overflow: hidden; border-radius: 25px;
      }
      #parent { position: relative; left: 40px; top: 40px; width: 180px;
               height: 180px; overflow: hidden;
      }
      #child { position: relative; left: -10px; top: -10px; width: 180px;
               height: 180px; background-color: green;
               will-change: transform;
      }
    </style>
    <div id='grandparent'>
      <div id='parent'>
        <div id='child'></div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  ASSERT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  ASSERT_TRUE(child_mapping->AncestorClippingMaskLayer());
  auto layer_size = child_mapping->AncestorClippingMaskLayer()->Size();
  EXPECT_EQ(160, layer_size.width());
  EXPECT_EQ(160, layer_size.height());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskNotRequiredByBorderRadiusInside) {
  // Verify that we do not create the mask layer when the child is contained
  // within the rounded rect clip.
  SetBodyInnerHTML(R"HTML(
    <style>
      #ancestor {
        width: 100px; height: 100px; overflow: hidden; border-radius: 5px;
      }
      #child { position: relative; left: 10px; top: 10px; width: 80px;
               height: 80px; background-color: green;
               will-change: transform;
      }
    </style>
    <div id='ancestor'>
      <div id='child'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskNotRequiredByBorderRadiusOutside) {
  // Verify that we do not create the mask layer when the child is outside
  // the ancestors rectangular clip.
  SetBodyInnerHTML(R"HTML(
    <style>
      #ancestor {
        width: 100px; height: 100px; overflow: hidden; border-radius: 5px;
      }
      #child { position: relative; left: 110px; top: 10px; width: 80px;
               height: 80px; background-color: green;
               will-change: transform;
    }
    </style>
    <div id='ancestor'>
      <div id='child'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ASSERT_TRUE(ancestor);
  PaintLayer* ancestor_paint_layer =
      ToLayoutBoxModelObject(ancestor->GetLayoutObject())->Layer();
  ASSERT_TRUE(ancestor_paint_layer);

  CompositedLayerMapping* ancestor_mapping =
      ancestor_paint_layer->GetCompositedLayerMapping();
  ASSERT_FALSE(ancestor_mapping);

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskRequiredDueToScaleUp) {
  // Verify that we include the mask when the untransformed child does not
  // intersect the border radius but the transformed child does. Here the
  // child is inside the parent and scaled to expand to be clipped.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent { position: relative; left: 40px; top: 40px; width: 120px;
               height: 120px; overflow: hidden; border-radius: 10px
      }
      #child { position: relative; left: 32px; top: 32px; width: 56px;
               height: 56px; background-color: green;
               transform: scale3d(2, 2, 1);
               will-change: transform;
      }
    </style>
    <div id='parent'>
      <div id='child'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskNotRequiredDueToScaleDown) {
  // Verify that we exclude the mask when the untransformed child does
  // intersect the border radius but the transformed child does not. Here the
  // child is bigger than the parent and scaled down such that it does not
  // need a mask.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent { position: relative; left: 40px; top: 40px; width: 120px;
               height: 120px; overflow: hidden; border-radius: 10px
      }
      #child { position: relative; left: -10px; top: -10px; width: 140px;
               height: 140px; background-color: green;
               transform: scale3d(0.5, 0.5, 1);
               will-change: transform;
      }
    </style>
    <div id='parent'>
      <div id='child'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskRequiredDueToTranslateInto) {
  // Verify that we include the mask when the untransformed child does not
  // intersect the border radius but the transformed child does. Here the
  // child is outside the parent and translated to be clipped.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent { position: relative; left: 40px; top: 40px; width: 120px;
               height: 120px; overflow: hidden; border-radius: 10px
      }
      #child { position: relative; left: 140px; top: 140px; width: 100px;
               height: 100px; background-color: green;
               transform: translate(-120px, -120px);
               will-change: transform;
      }
    </style>
    <div id='parent'>
      <div id='child'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskNotRequiredDueToTranslateOut) {
  // Verify that we exclude the mask when the untransformed child does
  // intersect the border radius but the transformed child does not. Here the
  // child is inside the parent and translated outside.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent { position: relative; left: 40px; top: 40px; width: 120px;
               height: 120px; overflow: hidden; border-radius: 10px
      }
      #child { position: relative; left: 15px; top: 15px; width: 100px;
               height: 100px; background-color: green;
               transform: translate(110px, 110px);
               will-change: transform;
      }
    </style>
    <div id='parent'>
      <div id='child'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_FALSE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskRequiredDueToRotation) {
  // Verify that we include the mask when the untransformed child does not
  // intersect the border radius but the transformed child does. Here the
  // child is just within the mask-not-required area but when rotated requires
  // a mask.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent { position: relative; left: 40px; top: 40px; width: 120px;
               height: 120px; overflow: hidden; border-radius: 10px
      }
      #child { position: relative; left: 11px; top: 11px; width: 98px;
               height: 98px; background-color: green;
               transform: rotate3d(0, 0, 1, 5deg);
               will-change: transform;
      }
    </style>
    <div id='parent'>
      <div id='child'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  EXPECT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskRequiredByBorderRadiusWithCompositedDescendant) {
  // This case has the child and grandchild within the ancestors and would
  // in principle not need a mask, but does because we cannot efficiently
  // check the bounds of the composited descendant for intersection with the
  // border.
  SetBodyInnerHTML(R"HTML(
    <style>
      #grandparent {
        width: 200px; height: 200px; overflow: hidden; border-radius: 25px;
      }
      #parent { position: relative; left: 30px; top: 30px; width: 140px;
               height: 140px; overflow: hidden; will-change: transform;
      }
      #child { position: relative; left: 10px; top: 10px; width: 120px;
               height: 120px; will-change: transform;
      }
    </style>
    <div id='grandparent'>
      <div id='parent'>
        <div id='child'></div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* parent = GetDocument().getElementById("parent");
  ASSERT_TRUE(parent);
  PaintLayer* parent_paint_layer =
      ToLayoutBoxModelObject(parent->GetLayoutObject())->Layer();
  ASSERT_TRUE(parent_paint_layer);
  CompositedLayerMapping* parent_mapping =
      parent_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(parent_mapping);
  EXPECT_TRUE(parent_mapping->AncestorClippingLayer());
  EXPECT_TRUE(parent_mapping->AncestorClippingLayer()->MaskLayer());
  EXPECT_TRUE(parent_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT,
       AncestorClipMaskGrandparentBorderRadiusCompositedDescendant) {
  // This case has the child clipped by the grandparent border radius but not
  // the parent, and does not itself require a mask to clip to the grandparent.
  // But the child has it's own composited child, so we force the mask in case
  // the child's child needs it.
  SetBodyInnerHTML(R"HTML(
    <style>
      #grandparent {
        width: 200px; height: 200px; overflow: hidden; border-radius: 25px;
      }
      #parent { position: relative; left: 30px; top: 30px; width: 140px;
               height: 140px; overflow: hidden;
      }
      #child { position: relative; left: 10px; top: 10px; width: 120px;
               height: 120px; will-change: transform;
      }
      #grandchild { position: relative; left: 10px; top: 10px; width: 200px;
               height: 200px; will-change: transform;
      }
    </style>
    <div id='grandparent'>
      <div id='parent'>
        <div id='child'>
          <div id='grandchild'></div>
        </div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* child = GetDocument().getElementById("child");
  ASSERT_TRUE(child);
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  ASSERT_TRUE(child_paint_layer);
  CompositedLayerMapping* child_mapping =
      child_paint_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(child_mapping);
  ASSERT_TRUE(child_mapping->AncestorClippingLayer());
  EXPECT_TRUE(child_mapping->AncestorClippingLayer()->MaskLayer());
  ASSERT_TRUE(child_mapping->AncestorClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTest, StickyPositionMainThreadOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>.composited { backface-visibility: hidden; }
    #scroller { overflow: auto; height: 200px; width: 200px; }
    .container { height: 500px; }
    .innerPadding { height: 10px; }
    #sticky { position: sticky; top: 25px; height: 50px; }</style>
    <div id='scroller' class='composited'>
      <div class='composited container'>
        <div class='composited container'>
          <div class='innerPadding'></div>
          <div id='sticky' class='composited'></div>
      </div></div></div>
  )HTML");

  PaintLayer* sticky_layer =
      ToLayoutBox(GetLayoutObjectByElementId("sticky"))->Layer();
  CompositedLayerMapping* sticky_mapping =
      sticky_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(sticky_mapping);

  // Now scroll the page - this should increase the main thread offset.
  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().Y());

  sticky_layer->SetNeedsCompositingInputsUpdate();
  EXPECT_TRUE(sticky_layer->NeedsCompositingInputsUpdate());
  GetDocument().View()->UpdateLifecycleToCompositingCleanPlusScrolling();
  EXPECT_FALSE(sticky_layer->NeedsCompositingInputsUpdate());
}

TEST_F(CompositedLayerMappingTest, StickyPositionNotSquashed) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: auto; height: 200px; }
    #sticky1, #sticky2, #sticky3 {position: sticky; top: 0; width: 50px;
        height: 50px; background: rgba(0, 128, 0, 0.5);}
    #sticky1 {backface-visibility: hidden;}
    .spacer {height: 2000px;}
    </style>
    <div id='scroller'>
      <div id='sticky1'></div>
      <div id='sticky2'></div>
      <div id='sticky3'></div>
      <div class='spacer'></div>
    </div>
  )HTML");

  PaintLayer* sticky1 =
      ToLayoutBlock(GetLayoutObjectByElementId("sticky1"))->Layer();
  PaintLayer* sticky2 =
      ToLayoutBlock(GetLayoutObjectByElementId("sticky2"))->Layer();
  PaintLayer* sticky3 =
      ToLayoutBlock(GetLayoutObjectByElementId("sticky3"))->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, sticky1->GetCompositingState());
  EXPECT_EQ(kNotComposited, sticky2->GetCompositingState());
  EXPECT_EQ(kNotComposited, sticky3->GetCompositingState());

  PaintLayer* scroller =
      ToLayoutBlock(GetLayoutObjectByElementId("scroller"))->Layer();
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().Y(), 100));
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Now that sticky2 and sticky3 overlap sticky1 they will be promoted, but
  // they should not be squashed into the same layer because they scroll with
  // respect to each other.
  EXPECT_EQ(kPaintsIntoOwnBacking, sticky1->GetCompositingState());
  EXPECT_EQ(kPaintsIntoOwnBacking, sticky2->GetCompositingState());
  EXPECT_EQ(kPaintsIntoOwnBacking, sticky3->GetCompositingState());
}

TEST_F(CompositedLayerMappingTest,
       LayerPositionForStickyElementInCompositedScroller) {
  SetBodyInnerHTML(R"HTML(
    <style>
     .scroller { overflow: scroll; width: 200px; height: 600px; }
     .composited { will-change:transform; }
     .perspective { perspective: 150px; }
     .box { position: sticky; width: 185px; height: 50px; top: 0px; }
     .container { width: 100%; height: 1000px; }
    </style>
    <div id='scroller' class='composited scroller'>
     <div class='composited container'>
      <div id='sticky' class='perspective box'></div>
     </div>
    </div>
  )HTML");

  LayoutBoxModelObject* sticky =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky"));
  CompositedLayerMapping* mapping =
      sticky->Layer()->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  GraphicsLayer* main_graphics_layer = mapping->MainGraphicsLayer();
  GraphicsLayer* child_transform_layer = mapping->ChildTransformLayer();

  ASSERT_TRUE(main_graphics_layer);
  ASSERT_TRUE(child_transform_layer);

  PaintLayer* scroller =
      ToLayoutBlock(GetLayoutObjectByElementId("scroller"))->Layer();
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().Y(), 100));
  GetDocument().View()->UpdateAllLifecyclePhases();

  // On the blink side, a sticky offset of (0, 100) should have been applied to
  // the sticky element.
  LayoutSize blink_sticky_offset = sticky->StickyPositionOffset();
  EXPECT_FLOAT_EQ(0, blink_sticky_offset.Width());
  EXPECT_FLOAT_EQ(100, blink_sticky_offset.Height());

  // On the CompositedLayerMapping side however, the offset should have been
  // removed so that the compositor can take care of it.
  EXPECT_FLOAT_EQ(0, main_graphics_layer->GetPosition().x());
  EXPECT_FLOAT_EQ(0, main_graphics_layer->GetPosition().y());

  // The child transform layer for the perspective shifting should also not be
  // moved by the sticky offset.
  EXPECT_FLOAT_EQ(0, child_transform_layer->GetPosition().x());
  EXPECT_FLOAT_EQ(0, child_transform_layer->GetPosition().y());
}

TEST_F(CompositedLayerMappingTest,
       LayerPositionForStickyElementInNonCompositedScroller) {
  SetBodyInnerHTML(R"HTML(
    <style>
     .scroller { overflow: scroll; width: 200px; height: 600px; }
     .composited { will-change:transform; }
     .box { position: sticky; width: 185px; height: 50px; top: 0px; }
     .container { width: 100%; height: 1000px; }
    </style>
    <div id='scroller' class='scroller'>
     <div class='composited container'>
      <div id='sticky' class='box'></div>
     </div>
    </div>
  )HTML");

  CompositedLayerMapping* mapping =
      ToLayoutBlock(GetLayoutObjectByElementId("sticky"))
          ->Layer()
          ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  GraphicsLayer* main_graphics_layer = mapping->MainGraphicsLayer();

  PaintLayer* scroller =
      ToLayoutBlock(GetLayoutObjectByElementId("scroller"))->Layer();
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().Y(), 100));
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FLOAT_EQ(0, main_graphics_layer->GetPosition().x());
  EXPECT_FLOAT_EQ(100, main_graphics_layer->GetPosition().y());
}

TEST_F(CompositedLayerMappingTest,
       TransformedRasterizationDisallowedForDirectReasons) {
  // This test verifies layers with direct compositing reasons won't have
  // transformed rasterization, i.e. should raster in local space.
  SetBodyInnerHTML(R"HTML(
    <div id='target1' style='transform:translateZ(0);'>foo</div>
    <div id='target2' style='will-change:opacity;'>bar</div>
    <div id='target3' style='backface-visibility:hidden;'>ham</div>
  )HTML");

  {
    LayoutObject* target = GetLayoutObjectByElementId("target1");
    ASSERT_TRUE(target && target->IsBox());
    PaintLayer* target_layer = ToLayoutBox(target)->Layer();
    GraphicsLayer* target_graphics_layer =
        target_layer ? target_layer->GraphicsLayerBacking() : nullptr;
    ASSERT_TRUE(target_graphics_layer);
    EXPECT_FALSE(target_graphics_layer->ContentLayer()
                     ->transformed_rasterization_allowed());
  }
  {
    LayoutObject* target = GetLayoutObjectByElementId("target2");
    ASSERT_TRUE(target && target->IsBox());
    PaintLayer* target_layer = ToLayoutBox(target)->Layer();
    GraphicsLayer* target_graphics_layer =
        target_layer ? target_layer->GraphicsLayerBacking() : nullptr;
    ASSERT_TRUE(target_graphics_layer);
    EXPECT_FALSE(target_graphics_layer->ContentLayer()
                     ->transformed_rasterization_allowed());
  }
  {
    LayoutObject* target = GetLayoutObjectByElementId("target3");
    ASSERT_TRUE(target && target->IsBox());
    PaintLayer* target_layer = ToLayoutBox(target)->Layer();
    GraphicsLayer* target_graphics_layer =
        target_layer ? target_layer->GraphicsLayerBacking() : nullptr;
    ASSERT_TRUE(target_graphics_layer);
    EXPECT_FALSE(target_graphics_layer->ContentLayer()
                     ->transformed_rasterization_allowed());
  }
}

TEST_F(CompositedLayerMappingTest, TransformedRasterizationForInlineTransform) {
  // This test verifies we allow layers that are indirectly composited due to
  // an inline transform (but no direct reason otherwise) to raster in the
  // device space for higher quality.
  SetBodyInnerHTML(R"HTML(
    <div style='will-change:transform; width:500px;
    height:20px;'>composited</div>
    <div id='target' style='transform:translate(1.5px,-10.5px);
    width:500px; height:20px;'>indirectly composited due to inline
    transform</div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  ASSERT_TRUE(target && target->IsBox());
  PaintLayer* target_layer = ToLayoutBox(target)->Layer();
  GraphicsLayer* target_graphics_layer =
      target_layer ? target_layer->GraphicsLayerBacking() : nullptr;
  ASSERT_TRUE(target_graphics_layer);
  EXPECT_TRUE(target_graphics_layer->ContentLayer()
                  ->transformed_rasterization_allowed());
}

// This tests that when the scroller becomes no longer scrollable if a sticky
// element is promoted for another reason we do remove its composited sticky
// constraint as it doesn't need to move on the compositor.
TEST_F(CompositedLayerMappingTestWithoutBGPT,
       CompositedStickyConstraintRemovedAndAdded) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .scroller { overflow: auto; height: 200px; }
    .sticky { position: sticky; top: 0; width: 10px; height: 10px; }
    .composited { will-change: transform; }
    </style>
    <div class='composited scroller'>
      <div id='sticky' class='composited sticky'></div>
      <div id='spacer' style='height: 2000px;'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();
  PaintLayer* sticky_layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky"))->Layer();
  EXPECT_TRUE(sticky_layer->GraphicsLayerBacking()
                  ->CcLayer()
                  ->sticky_position_constraint()
                  .is_sticky);

  // Make the scroller no longer scrollable.
  GetDocument().getElementById("spacer")->setAttribute(HTMLNames::styleAttr,
                                                       "height: 0;");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // The sticky position element is composited due to a compositing trigger but
  // should no longer have a sticky position constraint on the compositor.
  sticky_layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky"))->Layer();
  EXPECT_FALSE(sticky_layer->GraphicsLayerBacking()
                   ->CcLayer()
                   ->sticky_position_constraint()
                   .is_sticky);

  // Make the scroller scrollable again.
  GetDocument().getElementById("spacer")->setAttribute(HTMLNames::styleAttr,
                                                       "height: 2000px;");
  GetDocument().View()->UpdateAllLifecyclePhases();

  sticky_layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky"))->Layer();
  EXPECT_TRUE(sticky_layer->GraphicsLayerBacking()
                  ->CcLayer()
                  ->sticky_position_constraint()
                  .is_sticky);
}

TEST_F(CompositedLayerMappingTest, ScrollingContainerBoundsChange) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { width: 0; height: 0; }
      body { margin: 0; }
      #scroller { overflow-y: scroll; }
      #content {
        width: 100px;
        height: 100px;
        margin-top: 50px;
        margin-bottom: -50px;
      }
    </style>
    <div id='scroller'>
      <div id='content'></div>
    </div
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* scrollerElement = GetDocument().getElementById("scroller");
  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();

  cc::Layer* scrolling_layer = scrollable_area->LayerForScrolling()->CcLayer();
  EXPECT_EQ(0, scrolling_layer->CurrentScrollOffset().y());
  EXPECT_EQ(150, scrolling_layer->bounds().height());
  EXPECT_EQ(100, scrolling_layer->scroll_container_bounds().height());

  scrollerElement->setScrollTop(300);
  scrollerElement->setAttribute(HTMLNames::styleAttr, "max-height: 25px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(50, scrolling_layer->CurrentScrollOffset().y());
  EXPECT_EQ(150, scrolling_layer->bounds().height());
  EXPECT_EQ(25, scrolling_layer->scroll_container_bounds().height());

  scrollerElement->setAttribute(HTMLNames::styleAttr, "max-height: 300px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(50, scrolling_layer->CurrentScrollOffset().y());
  EXPECT_EQ(150, scrolling_layer->bounds().height());
  EXPECT_EQ(100, scrolling_layer->scroll_container_bounds().height());
}

TEST_F(CompositedLayerMappingTest, MainFrameLayerBackgroundColor) {
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::kWhite, GetDocument().View()->BaseBackgroundColor());
  auto* view_layer =
      GetDocument().GetLayoutView()->Layer()->GraphicsLayerBacking();
  EXPECT_EQ(Color::kWhite, view_layer->BackgroundColor());

  Color base_background(255, 0, 0);
  GetDocument().View()->SetBaseBackgroundColor(base_background);
  GetDocument().body()->setAttribute(HTMLNames::styleAttr,
                                     "background: rgba(0, 255, 0, 0.5)");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(base_background, GetDocument().View()->BaseBackgroundColor());
  EXPECT_EQ(Color(127, 128, 0, 255), view_layer->BackgroundColor());
}

TEST_F(CompositedLayerMappingTest, ScrollingLayerBackgroundColor) {
  SetBodyInnerHTML(R"HTML(
    <style>.color {background-color: blue}</style>
    <div id='target' style='width: 100px; height: 100px;
         overflow: scroll; will-change: transform'>
      <div style='height: 200px'></div>
    </div>
  )HTML");

  auto* target = GetDocument().getElementById("target");
  auto* mapping = ToLayoutBoxModelObject(target->GetLayoutObject())
                      ->Layer()
                      ->GetCompositedLayerMapping();
  auto* graphics_layer = mapping->MainGraphicsLayer();
  auto* scrolling_contents_layer = mapping->ScrollingContentsLayer();
  ASSERT_TRUE(graphics_layer);
  ASSERT_TRUE(scrolling_contents_layer);
  EXPECT_EQ(Color::kTransparent, graphics_layer->BackgroundColor());
  EXPECT_EQ(Color::kTransparent, scrolling_contents_layer->BackgroundColor());

  target->setAttribute(HTMLNames::classAttr, "color");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(Color(0, 0, 255), graphics_layer->BackgroundColor());
  EXPECT_EQ(Color(0, 0, 255), scrolling_contents_layer->BackgroundColor());
}

TEST_F(CompositedLayerMappingTest, ClipPathNoChildContainmentLayer) {
  // This test verifies only the presence of clip path does not induce child
  // containment layer.
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width:100px; height:100px; clip-path:circle();'>
      <div style='will-change:transform; width:200px; height:200px;'></div>
    </div>
  )HTML");
  auto* mapping = ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))
                      ->Layer()
                      ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  ASSERT_FALSE(mapping->ClippingLayer());
}

TEST_F(CompositedLayerMappingTestWithoutBGPT, ForegroundLayerSizing) {
  // This test verifies the foreground layer is sized to the clip rect.
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='position:relative; z-index:0; width:100px;
    height:100px; border:10px solid black; overflow:hidden;'>
      <div style='width:200px; height:200px; background:green;'></div>
      <div style='position:relative; z-index:-1;
    will-change:transform;'></div>
    </div>
  )HTML");
  auto* mapping = ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))
                      ->Layer()
                      ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  EXPECT_EQ(gfx::Size(120, 120), mapping->MainGraphicsLayer()->Size());
  ASSERT_TRUE(mapping->ClippingLayer());
  EXPECT_EQ(gfx::PointF(10, 10), mapping->ClippingLayer()->GetPosition());
  EXPECT_EQ(gfx::Size(100, 100), mapping->ClippingLayer()->Size());
  ASSERT_TRUE(mapping->ForegroundLayer());
  EXPECT_EQ(gfx::PointF(0, 0), mapping->ForegroundLayer()->GetPosition());
  EXPECT_EQ(gfx::Size(100, 100), mapping->ForegroundLayer()->Size());
}

TEST_F(CompositedLayerMappingTest, ScrollLayerSizingSubpixelAccumulation) {
  // This test verifies that when subpixel accumulation causes snapping it
  // applies to both the scrolling and scrolling contents layers. Verify that
  // the mapping doesn't have any vertical scrolling introduced as a result of
  // the snapping behavior. https://crbug.com/801381.
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);

  // The values below are chosen so that the subpixel accumulation causes the
  // pixel snapped height to be increased relative to snapping without it.
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        margin: 0;
      }
      #scroller {
        position: relative;
        top: 0.5625px;
        width: 200px;
        height: 200.8125px;
        overflow: auto;
      }
      #space {
        width: 1000px;
        height: 200.8125px;
      }
    </style>
    <div id="scroller">
      <div id="space"></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();
  auto* mapping = ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"))
                      ->Layer()
                      ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  ASSERT_TRUE(mapping->ScrollingLayer());
  ASSERT_TRUE(mapping->ScrollingContentsLayer());
  EXPECT_EQ(mapping->ScrollingLayer()->Size().height(),
            mapping->ScrollingContentsLayer()->Size().height());
}

TEST_F(CompositedLayerMappingTest, SquashingScroll) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;
  SetHtmlInnerHTML(R"HTML(
    <style>
      * { margin: 0 }
    </style>
    <div id=target
        style='width: 200px; height: 200px; position: relative; will-change: transform'></div>
    <div id=squashed
        style='width: 200px; height: 200px; top: -200px; position: relative;'></div>
    <div style='width: 10px; height: 3000px'></div>
  )HTML");

  auto* squashed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashed"))->Layer();
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());
  EXPECT_EQ(
      LayoutPoint(),
      squashed->GroupedMapping()->SquashingOffsetFromTransformedAncestor());

  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 25),
                                                   kUserScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(
      LayoutPoint(),
      squashed->GroupedMapping()->SquashingOffsetFromTransformedAncestor());
}

TEST_F(CompositedLayerMappingTest, SquashingScrollInterestRect) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;
  SetHtmlInnerHTML(R"HTML(
    <style>
      * { margin: 0 }
    </style>
    <div id=target
        style='width: 200px; height: 200px; position: relative; will-change: transform'></div>
    <div id=squashed
        style='width: 200px; height: 6000px; top: -200px; position: relative;'></div>
  )HTML");

  auto* squashed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashed"))->Layer();
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());

  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 5000),
                                                   kUserScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(IntRect(0, 1000, 200, 5000),
            squashed->GroupedMapping()->SquashingLayer()->InterestRect());
}

TEST_F(CompositedLayerMappingTest,
       SquashingBoundsUnderCompositedScrollingWithTransform) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetHtmlInnerHTML(R"HTML(
    <div id=scroller style="transform: translateZ(0); overflow: scroll;
    width: 200px; height: 400px;">
      <div id=squashing
          style='width: 200px; height: 200px; position: relative; will-change:
transform'></div>
      <div id=squashed style="width: 200px; height: 6000px; top: -100px;
          position: relative;">
      </div>
    </div>
    )HTML");
  Element* scroller_element = GetDocument().getElementById("scroller");
  auto* scroller = scroller_element->GetLayoutObject();
  EXPECT_EQ(kPaintsIntoOwnBacking, scroller->GetCompositingState());

  auto* squashing =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashing"))->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, squashing->GetCompositingState());

  auto* squashed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashed"))->Layer();
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());

  scroller_element->setScrollTop(300);

  GetDocument().View()->UpdateAllLifecyclePhases();

  // 100px down from squashing's main graphics layer.
  EXPECT_EQ(FloatPoint(0, 100),
            squashed->GraphicsLayerBacking()->GetPosition());
}

}  // namespace blink
