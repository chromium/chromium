// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"

#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"

namespace blink {

// TODO(wangxianzhu): Though these tests don't directly apply in
// CompositeAfterPaint, we should ensure the cases are tested in
// CompositeAfterPaint mode if applicable.
class CompositedLayerMappingTest : public RenderingTest {
 public:
  CompositedLayerMappingTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  IntRect RecomputeInterestRect(const GraphicsLayer* graphics_layer) {
    return static_cast<CompositedLayerMapping&>(graphics_layer->Client())
        .RecomputeInterestRect(graphics_layer);
  }

  IntRect ComputeInterestRect(GraphicsLayer* graphics_layer,
                              IntRect previous_interest_rect) {
    return static_cast<CompositedLayerMapping&>(graphics_layer->Client())
        .ComputeInterestRect(graphics_layer, previous_interest_rect);
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

  static const GraphicsLayerPaintInfo* GetSquashedLayer(
      const Vector<GraphicsLayerPaintInfo>& squashed_layers,
      const PaintLayer& layer) {
    for (const auto& squashed_layer : squashed_layers) {
      if (squashed_layer.paint_layer == &layer)
        return &squashed_layer;
    }
    return nullptr;
  }

  const GraphicsLayerPaintInfo* GetNonScrollingSquashedLayer(
      const CompositedLayerMapping& mapping,
      const PaintLayer& layer) {
    return GetSquashedLayer(mapping.non_scrolling_squashed_layers_, layer);
  }

  const GraphicsLayerPaintInfo* GetSquashedLayerInScrollingContents(
      const CompositedLayerMapping& mapping,
      const PaintLayer& layer) {
    return GetSquashedLayer(mapping.squashed_layers_in_scrolling_contents_,
                            layer);
  }

 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

TEST_F(CompositedLayerMappingTest, SubpixelAccumulationChange) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='will-change: opacity; background: lightblue;
        position: relative; left: 0.4px; width: 100px; height: 100px'>
      <!-- This div would be snapped to a different pixel -->
      <div style='position: relative; left: 0.3px; width: 50px; height: 50px;
           background: green'></div>
    </div>
  )HTML");

  GetDocument().View()->SetTracksRasterInvalidations(true);
  Element* target = GetDocument().getElementById("target");
  target->SetInlineStyleProperty(CSSPropertyID::kLeft, "0.6px");
  UpdateAllLifecyclePhasesForTest();
  // Directly composited layers are not invalidated on subpixel accumulation
  // change.
  EXPECT_TRUE(target->GetLayoutBox()
                  ->Layer()
                  ->GraphicsLayerBacking()
                  ->GetRasterInvalidationTracking()
                  ->Invalidations()
                  .IsEmpty());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_F(CompositedLayerMappingTest,
       SubpixelAccumulationChangeUnderInvalidation) {
  ScopedPaintUnderInvalidationCheckingForTest test(true);
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='will-change: opacity; background: lightblue;
        position: relative; left: 0.4px; width: 100px; height: 100px'>
      <!-- This div will be snapped to a different pixel -->
      <div style='position: relative; left: 0.3px; width: 50px; height: 50px;
           background: green'></div>
    </div>
  )HTML");

  GetDocument().View()->SetTracksRasterInvalidations(true);
  Element* target = GetDocument().getElementById("target");
  target->SetInlineStyleProperty(CSSPropertyID::kLeft, "0.6px");
  UpdateAllLifecyclePhasesForTest();
  // Invalidate directly composited layers on subpixel accumulation change
  // when PaintUnderInvalidationChecking is enabled.
  EXPECT_FALSE(target->GetLayoutBox()
                   ->Layer()
                   ->GraphicsLayerBacking()
                   ->GetRasterInvalidationTracking()
                   ->Invalidations()
                   .IsEmpty());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_F(CompositedLayerMappingTest,
       SubpixelAccumulationChangeIndirectCompositing) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        background: lightblue;
        position: relative;
        top: -10px;
        left: 0.4px;
        width: 100px;
        height: 100px;
        transform: translateX(0);
        opacity: 0.4;
      }
      #child {
        position; relative;
        width: 100px;
        height: 100px;
        background: lightgray;
        will-change: transform;
        opacity: 0.6;
      }
    </style>
    <div id='target'>
      <div id='child'></div>
    </div>
  )HTML");

  GetDocument().View()->SetTracksRasterInvalidations(true);
  Element* target = GetDocument().getElementById("target");
  target->SetInlineStyleProperty(CSSPropertyID::kLeft, "0.6px");
  UpdateAllLifecyclePhasesForTest();
  // Invalidate indirectly composited layers on subpixel accumulation change.
  EXPECT_FALSE(target->GetLayoutBox()
                   ->Layer()
                   ->GraphicsLayerBacking()
                   ->GetRasterInvalidationTracking()
                   ->Invalidations()
                   .IsEmpty());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_F(CompositedLayerMappingTest, SimpleInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px; will-change: "
      "transform'></div>");

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GetCompositedLayerMapping());
  EXPECT_EQ(IntRect(0, 0, 200, 200),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, TallLayerInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform'></div>");

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
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

  UpdateAllLifecyclePhasesForTest();
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 8000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 2992, 200, 7008),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, TallNonCompositedScrolledLayerInterestRect) {
  SetHtmlInnerHTML(R"HTML(
    <div style='width: 200px; height: 11000px;'></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 8000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

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

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
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

  UpdateAllLifecyclePhasesForTest();
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(-5000, 0), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

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

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 200, 200),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, RotatedInterestRectNear90Degrees) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 10000px; height: 200px; will-change: "
      "transform; transform: rotateY(89.9999deg)'></div>");

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
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
        background: blue;
        will-change: transform;
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
      #target {
        will-change: transform;
      }
    </style>
    <div class='wrapper'>
      <div id='target' class='container'>
        <div class='posabs'></div>
        <div id='target class='posabs'></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
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

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 1202, 837),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, RotationInterestRect) {
  SetBodyInnerHTML(R"HTML(
      <style>
  .red_box {
    position: fixed;
    height: 100px;
    width: 100vh; /* height of view, after -90 rot */
    right: calc(16px - 50vh); /* 16 pixels above top of view, after -90 */
    top: calc(50vh - 16px); /* 16 pixels in from right side, after -90 rot */
    transform-origin: top;
    transform: rotate(-90deg);
    background-color: red;
    will-change: transform;
  }
  .blue_box {
    height: 30px;
    width: 600px;
    background: blue;
  }
</style>
<div class="red_box" id=target>
  <div class="blue_box"></div>
</div>

  )HTML");
  GetFrame().View()->Resize(2000, 3000);

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 3000, 100),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, 3D90DegRotatedTallInterestRect) {
  // It's rotated 90 degrees about the X axis, which means its visual content
  // rect is empty, and so the interest rect is the default (0, 0, 4000, 4000)
  // intersected with the layer bounds.
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform; transform: rotateY(90deg)'></div>");

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 200, 4000),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, 3D45DegRotatedTallInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform; transform: rotateY(45deg)'></div>");

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 200, 6226),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, RotatedTallInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 10000px; will-change: "
      "transform; transform: rotateZ(45deg)'></div>");

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  ASSERT_TRUE(!!paint_layer->GraphicsLayerBacking());
  EXPECT_EQ(IntRect(0, 0, 200, 4000),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
}

TEST_F(CompositedLayerMappingTest, WideLayerInterestRect) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 10000px; height: 200px; will-change: "
      "transform'></div>");

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
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

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
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

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
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

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
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

  UpdateAllLifecyclePhasesForTest();
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  // Offscreen layers are painted as usual.
  EXPECT_EQ(IntRect(0, 0, 4001, 4001),
            RecomputeInterestRect(paint_layer->GraphicsLayerBacking()));
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

  UpdateAllLifecyclePhasesForTest();
  GraphicsLayer* root_scrolling_layer =
      GetDocument().GetLayoutView()->Layer()->GraphicsLayerBacking();
  EXPECT_EQ(IntRect(0, 0, 800, 4600),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  // Still use the previous interest rect because the recomputed rect hasn't
  // changed enough.
  EXPECT_EQ(IntRect(0, 0, 800, 4900),
            RecomputeInterestRect(root_scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 800, 4600),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 600), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  // Use recomputed interest rect because it changed enough.
  EXPECT_EQ(IntRect(0, 0, 800, 5200),
            RecomputeInterestRect(root_scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 800, 5200),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 5400), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(0, 1400, 800, 8600),
            RecomputeInterestRect(root_scrolling_layer));
  EXPECT_EQ(IntRect(0, 1400, 800, 8600),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 9000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  // Still use the previous interest rect because it contains the recomputed
  // interest rect.
  EXPECT_EQ(IntRect(0, 5000, 800, 5000),
            RecomputeInterestRect(root_scrolling_layer));
  EXPECT_EQ(IntRect(0, 1400, 800, 8600),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 2000), mojom::blink::ScrollType::kProgrammatic);
  // Use recomputed interest rect because it changed enough.
  UpdateAllLifecyclePhasesForTest();
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

  UpdateAllLifecyclePhasesForTest();
  GraphicsLayer* root_scrolling_layer =
      GetDocument().GetLayoutView()->Layer()->GraphicsLayerBacking();
  EXPECT_EQ(IntRect(0, 0, 800, 4600),
            PreviousInterestRect(root_scrolling_layer));

  GetDocument().View()->SetFrameRect(IntRect(0, 0, 800, 60));
  UpdateAllLifecyclePhasesForTest();
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

  UpdateAllLifecyclePhasesForTest();
  Element* scroller = GetDocument().getElementById("scroller");
  GraphicsLayer* scrolling_layer =
      scroller->GetLayoutBox()->Layer()->GraphicsLayerBacking();
  EXPECT_EQ(IntRect(0, 0, 400, 4400), PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(300);
  UpdateAllLifecyclePhasesForTest();
  // Still use the previous interest rect because the recomputed rect hasn't
  // changed enough.
  EXPECT_EQ(IntRect(0, 0, 400, 4700), RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 400, 4400), PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(600);
  UpdateAllLifecyclePhasesForTest();
  // Use recomputed interest rect because it changed enough.
  EXPECT_EQ(IntRect(0, 0, 400, 5000), RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 0, 400, 5000), PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(5600);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(0, 1600, 400, 8400),
            RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 1600, 400, 8400), PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(9000);
  UpdateAllLifecyclePhasesForTest();
  // Still use the previous interest rect because it contains the recomputed
  // interest rect.
  EXPECT_EQ(IntRect(0, 5000, 400, 5000),
            RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 1600, 400, 8400), PreviousInterestRect(scrolling_layer));

  scroller->setScrollTop(2000);
  // Use recomputed interest rect because it changed enough.
  UpdateAllLifecyclePhasesForTest();
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

  UpdateAllLifecyclePhasesForTest();
  Element* scroller = GetDocument().getElementById("scroller");
  GraphicsLayer* scrolling_layer =
      scroller->GetLayoutBox()->Layer()->GraphicsLayerBacking();

  scroller->setScrollTop(5400);
  UpdateAllLifecyclePhasesForTest();
  scroller->setScrollTop(9400);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(0, 5400, 400, 4600),
            RecomputeInterestRect(scrolling_layer));
  EXPECT_EQ(IntRect(0, 5400, 400, 4600), PreviousInterestRect(scrolling_layer));

  // Paint invalidation and repaint should change previous paint interest rect.
  GetDocument().getElementById("content")->setTextContent("Change");
  UpdateAllLifecyclePhasesForTest();
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
                grouped_mapping->NonScrollingSquashingLayer(), IntRect()));
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
                grouped_mapping->NonScrollingSquashingLayer(), IntRect()));
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
      ScrollOffset(0.0, 8000.0), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

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

  UpdateAllLifecyclePhasesForTest();

  // Scroll 7500 pixels down to bring the scrollable area to the bottom.
  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 7500.0), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

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

  UpdateAllLifecyclePhasesForTest();

  // Scroll 3000 pixels down to bring the scrollable area to somewhere in the
  // middle.
  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 3000.0), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

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

  UpdateAllLifecyclePhasesForTest();
  auto* fixed = ChildDocument().getElementById("fixed")->GetLayoutObject();
  auto* graphics_layer = fixed->EnclosingLayer()->GraphicsLayerBacking(fixed);

  // The graphics layer has dimensions 5400x300 but the interest rect clamps
  // this to the right-most 4000x4000 area.
  EXPECT_EQ(IntRect(1000, 0, 4400, 300), RecomputeInterestRect(graphics_layer));

  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 3000.0), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

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

  UpdateAllLifecyclePhasesForTest();
  auto* fixed = GetDocument().getElementById("fixed")->GetLayoutObject();
  auto* graphics_layer = fixed->EnclosingLayer()->GraphicsLayerBacking(fixed);
  EXPECT_EQ(IntRect(0, 500, 100, 4030), RecomputeInterestRect(graphics_layer));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 200.0), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

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
               width: 1px; height: 1px; position: absolute;
               backface-visibility: hidden; z-index: -1'></div>
      <div style='background-color: blue; width: 2000px; height: 2000px;
                  position: relative; top: 10px'></div>
    </div>
  )HTML");

  auto* mapping = To<LayoutBlock>(GetLayoutObjectByElementId("container"))
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
  EXPECT_TRUE(mapping->ForegroundLayer()->IsHitTestable());

  Element* negative_composited_child =
      GetDocument().getElementById("negative-composited-child");
  negative_composited_child->parentNode()->RemoveChild(
      negative_composited_child);
  UpdateAllLifecyclePhasesForTest();

  mapping = To<LayoutBlock>(GetLayoutObjectByElementId("container"))
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
  UpdateAllLifecyclePhasesForTest();

  Element* element = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      To<LayoutBoxModelObject>(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  // Decoration outline layer is created when composited scrolling.
  EXPECT_TRUE(paint_layer->HasCompositedLayerMapping());
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());

  CompositedLayerMapping* mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_TRUE(mapping->DecorationOutlineLayer());

  // No decoration outline layer is created when not composited scrolling.
  element->setAttribute(html_names::kStyleAttr, "overflow: visible;");
  UpdateAllLifecyclePhasesForTest();
  paint_layer = To<LayoutBoxModelObject>(element->GetLayoutObject())->Layer();
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
  UpdateAllLifecyclePhasesForTest();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      To<LayoutBoxModelObject>(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  CompositedLayerMapping* mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->DecorationOutlineLayer());

  // The decoration outline layer is created when composited scrolling
  // with an outline drawn over the composited scrolling region.
  scroller->setAttribute(html_names::kStyleAttr, "outline-offset: -2px;");
  UpdateAllLifecyclePhasesForTest();
  paint_layer = To<LayoutBoxModelObject>(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(mapping->DecorationOutlineLayer());

  // The decoration outline layer is destroyed when the scrolling region
  // will not be covered up by the outline.
  scroller->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  paint_layer = To<LayoutBoxModelObject>(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);

  mapping = paint_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->DecorationOutlineLayer());
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

  PaintLayer* sticky_layer = GetPaintLayerByElementId("sticky");
  CompositedLayerMapping* sticky_mapping =
      sticky_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(sticky_mapping);

  // Now scroll the page - this should increase the main thread offset.
  LayoutBoxModelObject* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().Y());

  sticky_layer->SetNeedsCompositingInputsUpdate();
  EXPECT_TRUE(sticky_layer->NeedsCompositingInputsUpdate());
  GetDocument().View()->UpdateLifecycleToCompositingCleanPlusScrolling(
      DocumentUpdateReason::kTest);
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

  auto* sticky1 =
      To<LayoutBlock>(GetLayoutObjectByElementId("sticky1"))->Layer();
  auto* sticky2 =
      To<LayoutBlock>(GetLayoutObjectByElementId("sticky2"))->Layer();
  auto* sticky3 =
      To<LayoutBlock>(GetLayoutObjectByElementId("sticky3"))->Layer();
  // All three sticky-pos elements are composited, because we composite
  // all sticky elements which stick to scrollers.
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
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("sticky"));
  CompositedLayerMapping* mapping =
      sticky->Layer()->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  GraphicsLayer* main_graphics_layer = mapping->MainGraphicsLayer();

  ASSERT_TRUE(main_graphics_layer);

  auto* scroller =
      To<LayoutBlock>(GetLayoutObjectByElementId("scroller"))->Layer();
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().Y(), 100));
  UpdateAllLifecyclePhasesForTest();

  // On the blink side, a sticky offset of (0, 100) should have been applied to
  // the sticky element.
  EXPECT_EQ(PhysicalOffset(0, 100), sticky->StickyPositionOffset());

  GraphicsLayer* root_scrolling_layer =
      GetDocument().GetLayoutView()->Layer()->GraphicsLayerBacking();
  const auto& root_layer_state = root_scrolling_layer->GetPropertyTreeState();
  const auto& sticky_layer_state = main_graphics_layer->GetPropertyTreeState();
  auto transform_from_sticky_to_root =
      GeometryMapper::SourceToDestinationProjection(
          sticky_layer_state.Transform(), root_layer_state.Transform());
  // Irrespective of if the ancestor scroller is composited or not, the sticky
  // position element should be at the same location.
  auto sticky_position_relative_to_root =
      transform_from_sticky_to_root.MapPoint(
          FloatPoint(main_graphics_layer->GetOffsetFromTransformNode()));
  EXPECT_FLOAT_EQ(8, sticky_position_relative_to_root.X());
  EXPECT_FLOAT_EQ(8, sticky_position_relative_to_root.Y());
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

  auto* mapping = To<LayoutBlock>(GetLayoutObjectByElementId("sticky"))
                      ->Layer()
                      ->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  GraphicsLayer* main_graphics_layer = mapping->MainGraphicsLayer();

  auto* scroller =
      To<LayoutBlock>(GetLayoutObjectByElementId("scroller"))->Layer();
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().Y(), 100));
  UpdateAllLifecyclePhasesForTest();

  GraphicsLayer* root_scrolling_layer =
      GetDocument().GetLayoutView()->Layer()->GraphicsLayerBacking();
  const auto& root_layer_state = root_scrolling_layer->GetPropertyTreeState();
  const auto& sticky_layer_state = main_graphics_layer->GetPropertyTreeState();
  auto transform_from_sticky_to_root =
      GeometryMapper::SourceToDestinationProjection(
          sticky_layer_state.Transform(), root_layer_state.Transform());
  // Irrespective of if the ancestor scroller is composited or not, the sticky
  // position element should be at the same location.
  auto sticky_position_relative_to_root =
      transform_from_sticky_to_root.MapPoint(
          FloatPoint(main_graphics_layer->GetOffsetFromTransformNode()));
  EXPECT_FLOAT_EQ(8, sticky_position_relative_to_root.X());
  EXPECT_FLOAT_EQ(8, sticky_position_relative_to_root.Y());
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

  UpdateAllLifecyclePhasesForTest();
  Element* scrollerElement = GetDocument().getElementById("scroller");
  auto* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();

  cc::Layer* scrolling_layer = scrollable_area->LayerForScrolling();
  auto element_id = scrollable_area->GetScrollElementId();
  auto& scroll_tree =
      scrolling_layer->layer_tree_host()->property_trees()->scroll_tree;
  EXPECT_EQ(0, scroll_tree.current_scroll_offset(element_id).y());
  EXPECT_EQ(150, scrolling_layer->bounds().height());
  auto* scroll_node = scroll_tree.FindNodeFromElementId(element_id);
  EXPECT_EQ(100, scroll_node->container_bounds.height());

  scrollerElement->setScrollTop(300);
  scrollerElement->setAttribute(html_names::kStyleAttr, "max-height: 25px;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(50, scroll_tree.current_scroll_offset(element_id).y());
  EXPECT_EQ(150, scrolling_layer->bounds().height());
  scroll_node = scroll_tree.FindNodeFromElementId(element_id);
  EXPECT_EQ(25, scroll_node->container_bounds.height());

  scrollerElement->setAttribute(html_names::kStyleAttr, "max-height: 300px;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(50, scroll_tree.current_scroll_offset(element_id).y());
  EXPECT_EQ(150, scrolling_layer->bounds().height());
  scroll_node = scroll_tree.FindNodeFromElementId(element_id);
  EXPECT_EQ(100, scroll_node->container_bounds.height());
}

TEST_F(CompositedLayerMappingTest, MainFrameLayerBackgroundColor) {
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(Color::kWhite, GetDocument().View()->BaseBackgroundColor());
  auto* view_cc_layer = ScrollingContentsCcLayerByScrollElementId(
      GetFrame().View()->RootCcLayer(),
      GetFrame().View()->LayoutViewport()->GetScrollElementId());
  EXPECT_EQ(SK_ColorWHITE, view_cc_layer->background_color());

  Color base_background(255, 0, 0);
  GetDocument().View()->SetBaseBackgroundColor(base_background);
  GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                     "background: rgba(0, 255, 0, 0.5)");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(base_background, GetDocument().View()->BaseBackgroundColor());
  EXPECT_EQ(SkColorSetARGB(255, 127, 128, 0),
            view_cc_layer->background_color());
}

TEST_F(CompositedLayerMappingTest, ScrollLayerSizingSubpixelAccumulation) {
  // This test verifies that when subpixel accumulation causes snapping it
  // applies to the scrolling contents layer. Verify that the mapping doesn't
  // have any vertical scrolling introduced as a result of the snapping
  // behavior. https://crbug.com/801381.
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
  UpdateAllLifecyclePhasesForTest();
  auto* mapping =
      GetPaintLayerByElementId("scroller")->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  ASSERT_TRUE(mapping->ScrollingContentsLayer());
  EXPECT_EQ(gfx::Size(200, 200), mapping->MainGraphicsLayer()->Size());
  EXPECT_EQ(gfx::Size(1000, 200), mapping->ScrollingContentsLayer()->Size());
}

TEST_F(CompositedLayerMappingTest, SquashingScrollInterestRect) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      * { margin: 0 }
    </style>
    <div id=target
        style='width: 200px; height: 200px; position: relative; will-change: transform'></div>
    <div id=squashed
        style='width: 200px; height: 6000px; top: -200px; position: relative;'></div>
  )HTML");

  auto* squashed = GetPaintLayerByElementId("squashed");
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());

  GetDocument().View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, 5000), mojom::blink::ScrollType::kUser);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(
      IntRect(0, 1000, 200, 5000),
      squashed->GroupedMapping()->SquashingLayer(*squashed)->InterestRect());
}

TEST_F(CompositedLayerMappingTest,
       SquashingBoundsUnderCompositedScrollingWithTransform) {
  SetHtmlInnerHTML(R"HTML(
    <div id=scroller style="will-change: transform; overflow: scroll;
        width: 200px; height: 400px;">
      <div id=squashing style='width: 200px; height: 200px; position: relative;
          will-change: transform'></div>
      <div id=squashed style="width: 200px; height: 6000px; top: -100px;
          position: relative;">
      </div>
    </div>
    )HTML");
  Element* scroller_element = GetDocument().getElementById("scroller");
  auto* scroller = scroller_element->GetLayoutObject();
  EXPECT_EQ(kPaintsIntoOwnBacking, scroller->GetCompositingState());

  auto* squashing = GetPaintLayerByElementId("squashing");
  EXPECT_EQ(kPaintsIntoOwnBacking, squashing->GetCompositingState());

  auto* squashed = GetPaintLayerByElementId("squashed");
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());

  scroller_element->setScrollTop(300);

  UpdateAllLifecyclePhasesForTest();

  ASSERT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());

  // 100px down from squashing's main graphics layer.
  EXPECT_EQ(IntPoint(0, 100),
            squashed->GraphicsLayerBacking()->GetOffsetFromTransformNode());
}

TEST_F(CompositedLayerMappingTest, ContentsNotOpaqueWithForegroundLayer) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
        position: relative;
        isolation: isolate;
      }
    </style>
    <div id='target' style='will-change: transform'>
      <div style='background: blue; z-index: -1; will-change: transform'></div>
      <div style='background: blue'></div>
    </div>
    )HTML");
  PaintLayer* target_layer = GetPaintLayerByElementId("target");
  CompositedLayerMapping* mapping = target_layer->GetCompositedLayerMapping();
  EXPECT_TRUE(mapping->ForegroundLayer());
  EXPECT_FALSE(mapping->MainGraphicsLayer()->ContentsOpaque());
}

TEST_F(CompositedLayerMappingTest, EmptyBoundsDoesntDrawContent) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      div {
        width: 100px;
        height: 0px;
        position: relative;
        isolation: isolate;
      }
    </style>
    <div id='target' style='will-change: transform; background: blue'>
    </div>
    )HTML");
  PaintLayer* target_layer = GetPaintLayerByElementId("target");
  CompositedLayerMapping* mapping = target_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->MainGraphicsLayer()->DrawsContent());
}

TEST_F(CompositedLayerMappingTest, TouchActionRectsWithoutContent) {
  SetBodyInnerHTML(
      "<div id='target' style='will-change: transform; width: 100px;"
      "    height: 100px; touch-action: none;'></div>");
  auto* box = To<LayoutBoxModelObject>(GetLayoutObjectByElementId("target"));
  auto* mapping = box->Layer()->GetCompositedLayerMapping();

  const auto& layer = mapping->MainGraphicsLayer()->CcLayer();
  auto expected = gfx::Rect(0, 0, 100, 100);
  EXPECT_EQ(layer.touch_action_region().GetAllRegions().bounds(), expected);

  EXPECT_TRUE(mapping->MainGraphicsLayer()->PaintsHitTest());

  // The only painted content for the main graphics layer is the touch-action
  // rect which is not sent to cc, so the cc::layer should not draw content.
  EXPECT_FALSE(layer.DrawsContent());
  EXPECT_FALSE(mapping->MainGraphicsLayer()->DrawsContent());
}

TEST_F(CompositedLayerMappingTest, ContentsOpaque) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
        position: relative;
        isolation: isolate;
      }
    </style>
    <div id='target' style='will-change: transform'>
      <div style='background: blue'></div>
    </div>
    )HTML");
  PaintLayer* target_layer = GetPaintLayerByElementId("target");
  CompositedLayerMapping* mapping = target_layer->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->ForegroundLayer());
  EXPECT_TRUE(mapping->MainGraphicsLayer()->ContentsOpaque());
}

TEST_F(CompositedLayerMappingTest, NullOverflowControlLayers) {
  SetHtmlInnerHTML("<div id='target' style='will-change: transform'></div>");
  CompositedLayerMapping* mapping =
      GetPaintLayerByElementId("target")->GetCompositedLayerMapping();
  EXPECT_FALSE(mapping->LayerForHorizontalScrollbar());
  EXPECT_FALSE(mapping->LayerForVerticalScrollbar());
  EXPECT_FALSE(mapping->LayerForScrollCorner());
}

TEST_F(CompositedLayerMappingTest, CompositedHiddenAnimatingLayer) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    @keyframes slide {
      0% { transform: translate3d(0px, 0px, 0px); }
      100% { transform: translate3d(100px, 0px, 1px); }
    }

    div {
      width: 123px;
      height: 234px;
      animation-duration: 2s;
      animation-name: slide;
      animation-iteration-count: infinite;
      animation-direction: alternate;
    }
    </style>
    <div id="animated"></div>
  )HTML");

  PaintLayer* animated = GetPaintLayerByElementId("animated");
  CompositedLayerMapping* mapping = animated->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  EXPECT_TRUE(mapping->MainGraphicsLayer()->GetCompositingReasons() &
              CompositingReason::kActiveTransformAnimation);

  // We still composite the animated layer even if visibility: hidden.
  // TODO(crbug.com/937573): Is this necessary?
  GetDocument()
      .getElementById("animated")
      ->setAttribute(html_names::kStyleAttr, "visibility: hidden");
  UpdateAllLifecyclePhasesForTest();
  mapping = animated->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  EXPECT_TRUE(mapping->MainGraphicsLayer()->GetCompositingReasons() &
              CompositingReason::kActiveTransformAnimation);
}

TEST_F(CompositedLayerMappingTest,
       RepaintScrollableAreaLayersInMainThreadScrolling) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      #scroller {
        width: 200px;
        height: 100px;
        overflow: scroll;
        opacity: 0.8; /*MainThreadScrollingReason::kHasOpacityAndLCDText*/
      }
      #child {
        width: 100px;
        height: 200px;
        transform: translate3d(0, 0, 0);
      }
      #uncorrelated {
        transform: translate3d(0, 0, 0);
        height: 100px;
        width: 100px;
        background-color: red;
      }
    </style>
    <div id="scroller">
      <div id="child">
      </div>
    </div>
    <div id="uncorrelated"></div>
  )HTML");

  PaintLayer* scroller = GetPaintLayerByElementId("scroller");

  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ASSERT_TRUE(scrollable_area->VerticalScrollbar()->IsOverlayScrollbar());

  ASSERT_FALSE(scrollable_area->NeedsCompositedScrolling());
  EXPECT_FALSE(scrollable_area->VerticalScrollbar()->FrameRect().IsEmpty());

  GraphicsLayer* vertical_scrollbar_layer =
      scrollable_area->GraphicsLayerForVerticalScrollbar();
  ASSERT_TRUE(vertical_scrollbar_layer);

  CompositedLayerMapping* mapping = scroller->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);

  // Input events, animations and DOM changes, etc, can trigger cc::ProxyMain::
  // BeginMainFrame, which may check if all graphics layers need repaint.
  //
  // We shouldn't repaint scrollable area layer which has no paint invalidation
  // in many uncorrelated BeginMainFrame scenes, such as moving mouse over the
  // non-scrollbar area, animating or DOM changes in another composited layer.
  GetDocument()
      .getElementById("uncorrelated")
      ->setAttribute(html_names::kStyleAttr, "width: 200px");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(mapping->NeedsRepaint(*vertical_scrollbar_layer));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "height: 50px");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(mapping->NeedsRepaint(*vertical_scrollbar_layer));
}

TEST_F(CompositedLayerMappingTest, IsolationClippingContainer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #hideable {
        overflow: hidden;
        height: 10px;
      }
      .isolation {
        contain: style layout;
        height: 100px;
      }
      .squash-container {
        will-change: transform;
      }
      .squashed {
        position: absolute;
        top: 0;
        left: 0;
        width: 100px;
        height: 100px;
      }
    </style>
    <div id="hideable">
      <div class="isolation" id="isolation_a">
        <div class="squash-container" id="squash_container_a">a</div>
        <div class="squashed"></div>
      </div>
      <div class="isolation">
        <div class="squash-container">b</div>
        <div class="squashed"></div>
      </div>
    </div>
  )HTML");

  Element* hideable = GetDocument().getElementById("hideable");
  hideable->SetInlineStyleProperty(CSSPropertyID::kOverflow, "visible");

  UpdateAllLifecyclePhasesForTest();

  auto* isolation_a = GetDocument().getElementById("isolation_a");
  auto* isolation_a_object = isolation_a->GetLayoutObject();

  auto* squash_container_a = GetDocument().getElementById("squash_container_a");
  PaintLayer* squash_container_a_layer =
      To<LayoutBoxModelObject>(squash_container_a->GetLayoutObject())->Layer();
  EXPECT_EQ(squash_container_a_layer->ClippingContainer(), isolation_a_object);
}

TEST_F(CompositedLayerMappingTest, SquashIntoScrollingContents) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(R"HTML(
    <div style="position: absolute; top: 0.5px; left: 0.75px; z-index: 1">
      <div style="height: 0.75px"></div>
      <div id="scroller" style="width: 100px; height: 100px; overflow: scroll;
           border: 10px solid blue">
        <div id="target1" style="position: relative; top: 10.5px; left: 5.5px;
             width: 10px; height: 10px; background: green"></div>
        <div style="height: 300px"></div>
        <div id="target2" style="position: relative; z-index: 2;
             width: 10px; height: 10px; background: green"></div>
      </div>
      <div style="position: absolute; z-index: 1; top: 50px;
           width: 10px; height: 10px; background: blue">
      </div>
    </div>
  )HTML");

  auto* scroller = GetPaintLayerByElementId("scroller");
  auto* target1 = GetPaintLayerByElementId("target1");
  auto* target2 = GetPaintLayerByElementId("target2");

  auto* scroller_mapping = scroller->GetCompositedLayerMapping();
  ASSERT_TRUE(scroller_mapping);
  EXPECT_EQ(IntSize(),
            scroller_mapping->MainGraphicsLayer()->OffsetFromLayoutObject());
  EXPECT_EQ(
      IntSize(10, 10),
      scroller_mapping->ScrollingContentsLayer()->OffsetFromLayoutObject());
  EXPECT_EQ(PhysicalOffset(LayoutUnit(-0.25), LayoutUnit(0.25)),
            scroller->SubpixelAccumulation());

  EXPECT_EQ(scroller_mapping, target1->GroupedMapping());
  EXPECT_EQ(scroller_mapping->ScrollingContentsLayer(),
            scroller_mapping->SquashingLayer(*target1));
  EXPECT_EQ(scroller_mapping->ScrollingContentsLayer(),
            target1->GraphicsLayerBacking());
  EXPECT_EQ(PhysicalOffset(LayoutUnit(0.25), LayoutUnit(-0.25)),
            target1->SubpixelAccumulation());
  const GraphicsLayerPaintInfo* target1_info =
      GetSquashedLayerInScrollingContents(*scroller_mapping, *target1);
  ASSERT_TRUE(target1_info);
  EXPECT_TRUE(target1_info->offset_from_layout_object_set);
  EXPECT_EQ(IntSize(-5, -11), target1_info->offset_from_layout_object);
  EXPECT_EQ(ClipRect(), target1_info->local_clip_rect_for_squashed_layer);

  // target2 can't be squashed because the absolute position div is between
  // the scrolling contents and target2.
  EXPECT_FALSE(target2->GroupedMapping());
  EXPECT_TRUE(target2->HasCompositedLayerMapping());
}

TEST_F(CompositedLayerMappingTest,
       SwitchSquashingBetweenScrollingAndNonScrolling) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(R"HTML(
    <style>.scroll { overflow: scroll; }</style>
    <div id="container"
         style="backface-visibility: hidden; width: 100px; height: 100px">
      <div id="squashed"
           style="z-index: 1; position: relative; width: 10px; height: 10px"></div>
      <div id="filler" style="height: 300px"></div>
    </div>
  )HTML");

  auto* container_element = GetDocument().getElementById("container");
  auto* container = container_element->GetLayoutBox()->Layer();
  auto* squashed = GetPaintLayerByElementId("squashed");
  auto* mapping = container->GetCompositedLayerMapping();
  ASSERT_TRUE(mapping);
  EXPECT_EQ(mapping, squashed->GroupedMapping());
  EXPECT_EQ(mapping->NonScrollingSquashingLayer(),
            squashed->GraphicsLayerBacking());
  EXPECT_EQ(mapping->NonScrollingSquashingLayer(),
            mapping->SquashingLayer(*squashed));
  EXPECT_TRUE(GetNonScrollingSquashedLayer(*mapping, *squashed));
  EXPECT_FALSE(GetSquashedLayerInScrollingContents(*mapping, *squashed));

  container_element->setAttribute(html_names::kClassAttr, "scroll");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(mapping, container->GetCompositedLayerMapping());
  EXPECT_EQ(mapping, squashed->GroupedMapping());
  EXPECT_EQ(mapping->ScrollingContentsLayer(),
            squashed->GraphicsLayerBacking());
  EXPECT_EQ(mapping->ScrollingContentsLayer(),
            mapping->SquashingLayer(*squashed));
  EXPECT_FALSE(GetNonScrollingSquashedLayer(*mapping, *squashed));
  EXPECT_TRUE(GetSquashedLayerInScrollingContents(*mapping, *squashed));

  container_element->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(mapping, container->GetCompositedLayerMapping());
  EXPECT_EQ(mapping->NonScrollingSquashingLayer(),
            squashed->GraphicsLayerBacking());
  EXPECT_EQ(mapping->NonScrollingSquashingLayer(),
            mapping->SquashingLayer(*squashed));
  EXPECT_TRUE(GetNonScrollingSquashedLayer(*mapping, *squashed));
  EXPECT_FALSE(GetSquashedLayerInScrollingContents(*mapping, *squashed));
}

// Unlike CompositingTest.WillChangeTransformHintInSVG, will-change hints on the
// SVG element itself should not opt into creating layers after paint.
TEST_F(CompositedLayerMappingTest, WillChangeTransformHintOnSVG) {
  ScopedCompositeSVGForTest enable_feature(true);
  SetBodyInnerHTML(R"HTML(
    <svg width="99" height="99" id="willChange" style="will-change: transform;">
      <rect width="100%" height="100%" fill="blue"></rect>
    </svg>
  )HTML");

  PaintLayer* paint_layer = GetPaintLayerByElementId("willChange");
  GraphicsLayer* graphics_layer = paint_layer->GraphicsLayerBacking();
  EXPECT_FALSE(graphics_layer->ShouldCreateLayersAfterPaint());
}

// Test that will-change changes inside SVG correctly update whether the
// graphics layer should create layers after paint.
TEST_F(CompositedLayerMappingTest, WillChangeTransformHintInSVGChanged) {
  ScopedCompositeSVGForTest enable_feature(true);
  SetBodyInnerHTML(R"HTML(
    <svg width="99" height="99" id="svg" style="will-change: transform;">
      <rect id="rect" width="100%" height="100%" fill="blue"></rect>
    </svg>
  )HTML");

  Element* svg = GetDocument().getElementById("svg");
  PaintLayer* paint_layer =
      To<LayoutBoxModelObject>(svg->GetLayoutObject())->Layer();
  EXPECT_FALSE(
      paint_layer->GraphicsLayerBacking()->ShouldCreateLayersAfterPaint());

  Element* rect = GetDocument().getElementById("rect");
  rect->setAttribute(html_names::kStyleAttr, "will-change: transform;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      paint_layer->GraphicsLayerBacking()->ShouldCreateLayersAfterPaint());

  rect->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(
      paint_layer->GraphicsLayerBacking()->ShouldCreateLayersAfterPaint());

  // Remove will-change from the svg element and perform the same tests. The
  // z-index just ensures a paint layer exists so the test is similar.
  svg->setAttribute(html_names::kStyleAttr, "z-index: 5;");
  UpdateAllLifecyclePhasesForTest();
  paint_layer = To<LayoutBoxModelObject>(svg->GetLayoutObject())->Layer();
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());

  rect->setAttribute(html_names::kStyleAttr, "will-change: transform;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      paint_layer->GraphicsLayerBacking()->ShouldCreateLayersAfterPaint());

  rect->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

}  // namespace blink
