// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using ::testing::ElementsAre;
using ::testing::Pointee;

class PaintLayerTest : public PaintControllerPaintTest {
 public:
  PaintLayerTest()
      : PaintControllerPaintTest(
            MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintLayerTest);

TEST_P(PaintLayerTest, ChildWithoutPaintLayer) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px;'></div>");

  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  PaintLayer* root_layer = GetLayoutView().Layer();

  EXPECT_EQ(nullptr, paint_layer);
  EXPECT_NE(nullptr, root_layer);
}

TEST_P(PaintLayerTest, CompositedBoundsAbsPosGrandchild) {
  // BoundingBoxForCompositing is not used in CAP mode.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
  SetBodyInnerHTML(
      " <div id='parent'><div id='absposparent'><div id='absposchild'>"
      " </div></div></div>"
      "<style>"
      "  #parent { position: absolute; z-index: 0; overflow: hidden;"
      "  background: lightgray; width: 150px; height: 150px;"
      "  will-change: transform; }"
      "  #absposparent { position: absolute; z-index: 0; }"
      "  #absposchild { position: absolute; top: 0px; left: 0px; height: 200px;"
      "  width: 200px; background: lightblue; }</style>");

  PaintLayer* parent_layer = GetPaintLayerByElementId("parent");
  // Since "absposchild" is clipped by "parent", it should not expand the
  // composited bounds for "parent" beyond its intrinsic size of 150x150.
  EXPECT_EQ(PhysicalRect(0, 0, 150, 150),
            parent_layer->BoundingBoxForCompositing());
}

TEST_P(PaintLayerTest, CompositedBoundsTransformedChild) {
  // TODO(chrishtr): fix this test for CAP
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id=parent style='overflow: scroll; will-change: transform'>
      <div class='target'
           style='position: relative; transform: skew(-15deg);'>
      </div>
      <div style='width: 1000px; height: 500px; background: lightgray'>
      </div>
    </div>
  )HTML");

  PaintLayer* parent_layer = GetPaintLayerByElementId("parent");
  EXPECT_EQ(PhysicalRect(0, 0, 784, 500),
            parent_layer->BoundingBoxForCompositing());
}

TEST_P(PaintLayerTest, RootLayerCompositedBounds) {
  SetBodyInnerHTML(
      "<style> body { width: 1000px; height: 1000px; margin: 0 } </style>");
  EXPECT_EQ(PhysicalRect(0, 0, 800, 600),
            GetLayoutView().Layer()->BoundingBoxForCompositing());
}

TEST_P(PaintLayerTest, RootLayerScrollBounds) {
  USE_NON_OVERLAY_SCROLLBARS();

  SetBodyInnerHTML(
      "<style> body { width: 1000px; height: 1000px; margin: 0 } </style>");
  PaintLayerScrollableArea* plsa = GetLayoutView().Layer()->GetScrollableArea();

  int scrollbarThickness = plsa->VerticalScrollbarWidth();
  EXPECT_EQ(scrollbarThickness, plsa->HorizontalScrollbarHeight());
  EXPECT_GT(scrollbarThickness, 0);

  EXPECT_EQ(ScrollOffset(200 + scrollbarThickness, 400 + scrollbarThickness),
            plsa->MaximumScrollOffset());

  EXPECT_EQ(IntRect(0, 0, 800 - scrollbarThickness, 600 - scrollbarThickness),
            plsa->VisibleContentRect());
  EXPECT_EQ(IntRect(0, 0, 800, 600),
            plsa->VisibleContentRect(kIncludeScrollbars));
}


TEST_P(PaintLayerTest, ScrollsWithViewportRelativePosition) {
  SetBodyInnerHTML("<div id='target' style='position: relative'></div>");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_FALSE(layer->FixedToViewport());
}

TEST_P(PaintLayerTest, ScrollsWithViewportFixedPosition) {
  SetBodyInnerHTML("<div id='target' style='position: fixed'></div>");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->FixedToViewport());
}

TEST_P(PaintLayerTest, ScrollsWithViewportFixedPositionInsideTransform) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateZ(0)'>
      <div id='target' style='position: fixed'></div>
    </div>
    <div style='width: 10px; height: 1000px'></div>
  )HTML");
  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_FALSE(layer->FixedToViewport());
}

TEST_P(PaintLayerTest, SticksToScrollerStickyPosition) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateZ(0)'>
      <div id='target' style='position: sticky; top: 0;'></div>
    </div>
    <div style='width: 10px; height: 1000px'></div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, SticksToScrollerNoAnchor) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateZ(0)'>
      <div id='target' style='position: sticky'></div>
    </div>
    <div style='width: 10px; height: 1000px'></div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_FALSE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, SticksToScrollerStickyPositionNoScroll) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateZ(0)'>
      <div id='target' style='position: sticky; top: 0;'></div>
    </div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, SticksToScrollerStickyPositionInsideScroller) {
  SetBodyInnerHTML(R"HTML(
    <div style='overflow:scroll; width: 100px; height: 100px;'>
      <div id='target' style='position: sticky; top: 0;'></div>
      <div style='width: 50px; height: 1000px;'></div>
    </div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(layer->SticksToScroller());
}

TEST_P(PaintLayerTest, CompositedScrollingNoNeedsRepaint) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id='scroll' style='width: 100px; height: 100px; overflow: scroll;
        will-change: transform'>
      <div id='content' style='position: relative; background: blue;
          width: 2000px; height: 2000px'></div>
    </div>
  )HTML");

  PaintLayer* scroll_layer = GetPaintLayerByElementId("scroll");
  EXPECT_EQ(kPaintsIntoOwnBacking, scroll_layer->GetCompositingState());

  PaintLayer* content_layer = GetPaintLayerByElementId("content");
  EXPECT_EQ(kNotComposited, content_layer->GetCompositingState());
  EXPECT_EQ(PhysicalOffset(), content_layer->LocationWithoutPositionOffset());

  scroll_layer->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(1000, 1000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(PhysicalOffset(0, 0),
            content_layer->LocationWithoutPositionOffset());
  EXPECT_EQ(
      LayoutSize(1000, 1000),
      content_layer->ContainingLayer()->PixelSnappedScrolledContentOffset());
  EXPECT_FALSE(content_layer->SelfNeedsRepaint());
  EXPECT_FALSE(scroll_layer->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerTest, NonCompositedScrollingNeedsRepaint) {
  // CAP scrolling raster invalidation decisions are made in
  // ContentLayerClientImpl::GenerateRasterInvalidations through
  // PaintArtifactCompositor.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
     /* to prevent the mock overlay scrollbar from affecting compositing. */
     ::-webkit-scrollbar { display: none; }
    </style>
    <div id='scroll' style='width: 100px; height: 100px; overflow: scroll'>
      <div id='content' style='position: relative; background: blue;
          width: 2000px; height: 2000px'></div>
    </div>
  )HTML");

  PaintLayer* scroll_layer = GetPaintLayerByElementId("scroll");
  EXPECT_EQ(kNotComposited, scroll_layer->GetCompositingState());

  PaintLayer* content_layer = GetPaintLayerByElementId("content");
  EXPECT_EQ(kNotComposited, scroll_layer->GetCompositingState());
  EXPECT_EQ(PhysicalOffset(), content_layer->LocationWithoutPositionOffset());

  scroll_layer->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(1000, 1000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(PhysicalOffset(0, 0),
            content_layer->LocationWithoutPositionOffset());
  EXPECT_EQ(
      LayoutSize(1000, 1000),
      content_layer->ContainingLayer()->PixelSnappedScrolledContentOffset());

  EXPECT_TRUE(scroll_layer->SelfNeedsRepaint());
  EXPECT_FALSE(content_layer->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerTest, HasNonIsolatedDescendantWithBlendMode) {
  SetBodyInnerHTML(R"HTML(
    <div id='stacking-grandparent' style='isolation: isolate'>
      <div id='stacking-parent' style='isolation: isolate'>
        <div id='non-stacking-parent' style='position:relative'>
          <div id='blend-mode' style='mix-blend-mode: overlay'>
          </div>
        </div>
      </div>
    </div>
  )HTML");
  PaintLayer* stacking_grandparent =
      GetPaintLayerByElementId("stacking-grandparent");
  PaintLayer* stacking_parent = GetPaintLayerByElementId("stacking-parent");
  PaintLayer* parent = GetPaintLayerByElementId("non-stacking-parent");

  EXPECT_TRUE(parent->HasNonIsolatedDescendantWithBlendMode());
  EXPECT_TRUE(stacking_parent->HasNonIsolatedDescendantWithBlendMode());
  EXPECT_FALSE(stacking_grandparent->HasNonIsolatedDescendantWithBlendMode());
  EXPECT_TRUE(parent->HasVisibleDescendant());
}

TEST_P(PaintLayerTest, HasStickyPositionDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='isolation: isolate'>
      <div id='child' style='position: sticky'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->HasStickyPositionDescendant());
  EXPECT_FALSE(child->HasStickyPositionDescendant());

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "position: relative");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(parent->HasStickyPositionDescendant());
  EXPECT_FALSE(child->HasStickyPositionDescendant());
}

TEST_P(PaintLayerTest, HasFixedPositionDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='isolation: isolate'>
      <div id='child' style='position: fixed'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child->HasFixedPositionDescendant());

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "position: relative");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child->HasFixedPositionDescendant());
}

TEST_P(PaintLayerTest, HasFixedAndStickyPositionDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='isolation: isolate'>
      <div id='child1' style='position: sticky'>
      </div>
      <div id='child2' style='position: fixed'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child1 = GetPaintLayerByElementId("child1");
  PaintLayer* child2 = GetPaintLayerByElementId("child2");
  EXPECT_TRUE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child1->HasFixedPositionDescendant());
  EXPECT_FALSE(child2->HasFixedPositionDescendant());
  EXPECT_TRUE(parent->HasStickyPositionDescendant());
  EXPECT_FALSE(child1->HasStickyPositionDescendant());
  EXPECT_FALSE(child2->HasStickyPositionDescendant());

  GetDocument().getElementById("child1")->setAttribute(html_names::kStyleAttr,
                                                       "position: relative");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child1->HasFixedPositionDescendant());
  EXPECT_FALSE(child2->HasFixedPositionDescendant());
  EXPECT_FALSE(parent->HasStickyPositionDescendant());
  EXPECT_FALSE(child1->HasStickyPositionDescendant());
  EXPECT_FALSE(child2->HasStickyPositionDescendant());

  GetDocument().getElementById("child2")->setAttribute(html_names::kStyleAttr,
                                                       "position: relative");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child1->HasFixedPositionDescendant());
  EXPECT_FALSE(child2->HasFixedPositionDescendant());
  EXPECT_FALSE(parent->HasStickyPositionDescendant());
  EXPECT_FALSE(child1->HasStickyPositionDescendant());
  EXPECT_FALSE(child2->HasStickyPositionDescendant());
}

TEST_P(PaintLayerTest, HasNonContainedAbsolutePositionDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='isolation: isolate'>
      <div id='child' style='position: relative'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");
  EXPECT_FALSE(parent->HasNonContainedAbsolutePositionDescendant());
  EXPECT_FALSE(child->HasNonContainedAbsolutePositionDescendant());

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "position: absolute");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(parent->HasNonContainedAbsolutePositionDescendant());
  EXPECT_FALSE(child->HasNonContainedAbsolutePositionDescendant());

  GetDocument().getElementById("parent")->setAttribute(html_names::kStyleAttr,
                                                       "position: relative");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->HasNonContainedAbsolutePositionDescendant());
  EXPECT_FALSE(child->HasNonContainedAbsolutePositionDescendant());
}

TEST_P(PaintLayerTest, HasSelfPaintingDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position: relative'>
      <div id='child' style='position: relative'>
        <div></div>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_TRUE(parent->HasSelfPaintingLayerDescendant());
  EXPECT_FALSE(child->HasSelfPaintingLayerDescendant());
}

TEST_P(PaintLayerTest, HasSelfPaintingDescendantNotSelfPainting) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position: relative'>
      <div id='child' style='overflow: auto'>
        <div></div>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_FALSE(parent->HasSelfPaintingLayerDescendant());
  EXPECT_FALSE(child->HasSelfPaintingLayerDescendant());
}

TEST_P(PaintLayerTest, HasSelfPaintingParentNotSelfPainting) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='overflow: auto'>
      <div id='child' style='position: relative'>
        <div></div>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_TRUE(parent->HasSelfPaintingLayerDescendant());
  EXPECT_FALSE(child->HasSelfPaintingLayerDescendant());
}

static const Vector<PaintLayer*>* LayersPaintingOverlayOverflowControlsAfter(
    const PaintLayer* layer) {
  return PaintLayerPaintOrderIterator(*layer->AncestorStackingContext(),
                                      kPositiveZOrderChildren)
      .LayersPaintingOverlayOverflowControlsAfter(layer);
}

// We need new enum and class to test the overlay overflow controls reordering,
// but we don't move the tests related to the new class to the bottom, which is
// behind all tests of the PaintLayerTest. Because it will make the git history
// hard to track.
enum OverlayType { kOverlayResizer, kOverlayScrollbars };

class ReorderOverlayOverflowControlsTest
    : public testing::WithParamInterface<std::tuple<unsigned, OverlayType>>,
      private ScopedCompositeAfterPaintForTest,
      public RenderingTest {
 public:
  ReorderOverlayOverflowControlsTest()
      : ScopedCompositeAfterPaintForTest(std::get<0>(GetParam()) &
                                         kCompositeAfterPaint),
        RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
  ~ReorderOverlayOverflowControlsTest() {
    // Must destruct all objects before toggling back feature flags.
    WebHeap::CollectAllGarbageForTesting();
  }

  OverlayType GetOverlayType() const { return std::get<1>(GetParam()); }

  void InitOverflowStyle(const char* id) {
    GetDocument().getElementById(id)->setAttribute(
        html_names::kStyleAttr, GetOverlayType() == kOverlayScrollbars
                                    ? "overflow : auto"
                                    : "overflow: hidden; resize: both");
    UpdateAllLifecyclePhasesForTest();
  }

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ReorderOverlayOverflowControlsTest,
    ::testing::Combine(::testing::Values(0, kCompositeAfterPaint),
                       ::testing::Values(kOverlayScrollbars, kOverlayResizer)));

TEST_P(ReorderOverlayOverflowControlsTest, StackedWithInFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        position: relative;
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='parent'>
      <div id='child' style='position: relative; height: 200px'></div>
    </div>
  )HTML");

  InitOverflowStyle("parent");

  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  if (GetOverlayType() == kOverlayScrollbars) {
    EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
    EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));
  } else {
    EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
    EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
    EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
                Pointee(ElementsAre(parent)));
  }

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
}

TEST_P(ReorderOverlayOverflowControlsTest, StackedWithOutOfFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #child {
        width: 200px;
        height: 200px;
      }
      #parent {
        position: relative;
        height: 100px;
      }
    </style>
    <div id='parent'>
      <div id='child' style='position: absolute'></div>
    </div>
  )HTML");

  InitOverflowStyle("parent");

  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "position: absolute");
  UpdateAllLifecyclePhasesForTest();
  child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
}

TEST_P(ReorderOverlayOverflowControlsTest, StackedWithZIndexDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        position: relative;
        height: 100px;
      }
      #child {
        position: absolute;
        width: 200px;
        height: 200px;
      }
    </style>
    <div id='parent'>
      <div id='child' style='z-index: 1'></div>
    </div>
  )HTML");

  InitOverflowStyle("parent");

  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "z-index: -1");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "z-index: 2");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       NestedStackedWithInFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #ancestor {
        position: relative;
        height: 100px;
      }
      #parent {
        height: 200px;
      }
      #child {
        position: relative;
        height: 300px;
      }
    </style>
    <div id='ancestor'>
      <div id='parent'>
        <div id="child"></div>
      </div>
    </div>
  )HTML");

  InitOverflowStyle("ancestor");
  InitOverflowStyle("parent");

  auto* ancestor = GetPaintLayerByElementId("ancestor");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(ancestor->NeedsReorderOverlayOverflowControls());
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent, ancestor)));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       NestedStackedWithOutOfFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #ancestor {
        position: relative;
        height: 100px;
      }
      #parent {
        position: absolute;
        width: 200px;
        height: 200px;
      }
      #child {
        position: absolute;
        width: 300px;
        height: 300px;
      }
    </style>
    <div id='ancestor'>
      <div id='parent'>
        <div id="child">
        </div>
      </div>
    </div>
  )HTML");

  InitOverflowStyle("ancestor");
  InitOverflowStyle("parent");

  auto* ancestor = GetPaintLayerByElementId("ancestor");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(ancestor->NeedsReorderOverlayOverflowControls());
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent, ancestor)));
}

TEST_P(ReorderOverlayOverflowControlsTest, MultipleChildren) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        width: 200px;
        height: 200px;
      }
      #parent {
        width: 100px;
        height: 100px;
      }
      #low-child {
        position: absolute;
        z-index: 1;
      }
      #middle-child {
        position: relative;
        z-index: 2;
      }
      #high-child {
        position: absolute;
        z-index: 3;
      }
    </style>
    <div id='parent'>
      <div id="low-child"></div>
      <div id="middle-child"></div>
      <div id="high-child"></div>
    </div>
  )HTML");

  InitOverflowStyle("parent");

  auto* parent = GetPaintLayerByElementId("parent");
  auto* low_child = GetPaintLayerByElementId("low-child");
  auto* middle_child = GetPaintLayerByElementId("middle-child");
  auto* high_child = GetPaintLayerByElementId("high-child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(low_child));
  // The highest contained child by parent is middle_child because the
  // absolute-position children are not contained.
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(middle_child),
              Pointee(ElementsAre(parent)));
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(high_child));

  std::string extra_style = GetOverlayType() == kOverlayScrollbars
                                ? "overflow: auto;"
                                : "overflow: hidden; resize: both;";
  std::string new_style = extra_style + "position: absolute; z-index: 1";
  GetDocument().getElementById("parent")->setAttribute(html_names::kStyleAttr,
                                                       new_style.c_str());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(low_child));
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(middle_child));
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(high_child));

  new_style = extra_style + "position: absolute;";
  GetDocument().getElementById("parent")->setAttribute(html_names::kStyleAttr,
                                                       new_style.c_str());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(low_child));
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(middle_child));
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(high_child),
              Pointee(ElementsAre(parent)));
}

TEST_P(ReorderOverlayOverflowControlsTest, NonStackedWithInFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='parent'>
      <div id='child' style='position: relative; height: 200px'></div>
    </div>
  )HTML");

  InitOverflowStyle("parent");

  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  if (GetOverlayType() == kOverlayResizer) {
    EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
    EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
                Pointee(ElementsAre(parent)));
  } else {
    EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
    EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));
  }

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "position: relative; width: 200px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       NonStackedWithZIndexInFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        height: 100px;
      }
      #child {
        position: relative;
        height: 200px;
      }
    </style>
    <div id='parent'>
      <div id='child' style='z-index: 1'></div>
    </div>
  )HTML");

  InitOverflowStyle("parent");

  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "z-index: -1");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "z-index: 2");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
}

TEST_P(ReorderOverlayOverflowControlsTest, NonStackedWithOutOfFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        height: 100px;
      }
      #child {
        position: absolute;
        width: 200px;
        height: 200px;
      }
    </style>
    <div id='parent'>
      <div id='child'></div>
    </div>
  )HTML");

  InitOverflowStyle("parent");

  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));
}

TEST_P(ReorderOverlayOverflowControlsTest, NonStackedWithNonStackedDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent'>
      <div id='child'></div>
    </div>
  )HTML");

  InitOverflowStyle("parent");
  InitOverflowStyle("child");

  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");

  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       NestedNonStackedWithInFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #ancestor {
        height: 100px;
      }
      #parent {
        height: 200px;
      }
      #child {
        position: relative;
        height: 300px;
      }
    </style>
    <div id='ancestor'>
      <div id='parent'>
        <div id='child'></div>
      </div>
    </div>
  )HTML");

  InitOverflowStyle("ancestor");
  InitOverflowStyle("parent");

  auto* ancestor = GetPaintLayerByElementId("ancestor");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(ancestor->NeedsReorderOverlayOverflowControls());
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent, ancestor)));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       NestedNonStackedWithOutOfFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #ancestor {
        height: 100px;
      }
      #parent {
        height: 200px;
      }
      #child {
        position: absolute;
        width: 300px;
        height: 300px;
      }
    </style>
    <div id='ancestor'>
      <div id='parent'>
        <div id='child'>
        </div>
      </div>
    </div>
  )HTML");

  InitOverflowStyle("ancestor");
  InitOverflowStyle("parent");

  auto* ancestor = GetPaintLayerByElementId("ancestor");
  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_FALSE(ancestor->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(child->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       AdjustAccessingOrderForSubtreeHighestLayers) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        width: 200px;
        height: 200px;
      }
      div > div {
        height: 300px;
      }
      #ancestor, #child_2 {
        position: relative;
      }
      #child_1 {
        position: absolute;
      }
    </style>
    <div id='ancestor'>
      <div id='child_1'></div>
      <div id='child_2'>
        <div id='descendant'></div>
      </div>
    </div>
  )HTML");

  InitOverflowStyle("ancestor");

  auto* ancestor = GetPaintLayerByElementId("ancestor");
  auto* child = GetPaintLayerByElementId("child_2");
  EXPECT_TRUE(ancestor->NeedsReorderOverlayOverflowControls());
  EXPECT_TRUE(LayersPaintingOverlayOverflowControlsAfter(child));
}

TEST_P(PaintLayerTest, SubsequenceCachingStackedLayers) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative'>
      <div id='child1' style='position: relative'>
        <div id='grandchild1' style='position: relative'></div>
      </div>
      <div id='child2' style='isolation: isolate'>
        <div id='grandchild2' style='position: relative'></div>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child1 = GetPaintLayerByElementId("child1");
  PaintLayer* child2 = GetPaintLayerByElementId("child2");
  PaintLayer* grandchild1 = GetPaintLayerByElementId("grandchild1");
  PaintLayer* grandchild2 = GetPaintLayerByElementId("grandchild2");

  EXPECT_FALSE(parent->SupportsSubsequenceCaching());
  EXPECT_FALSE(child1->SupportsSubsequenceCaching());
  EXPECT_TRUE(child2->SupportsSubsequenceCaching());
  EXPECT_FALSE(grandchild1->SupportsSubsequenceCaching());
  EXPECT_FALSE(grandchild2->SupportsSubsequenceCaching());

  GetDocument()
      .getElementById("grandchild1")
      ->setAttribute(html_names::kStyleAttr, "isolation: isolate");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(parent->SupportsSubsequenceCaching());
  EXPECT_FALSE(child1->SupportsSubsequenceCaching());
  EXPECT_TRUE(child2->SupportsSubsequenceCaching());
  EXPECT_TRUE(grandchild1->SupportsSubsequenceCaching());
  EXPECT_FALSE(grandchild2->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, SubsequenceCachingSVGRoot) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position: relative'>
      <svg id='svgroot' style='position: relative'></svg>
    </div>
  )HTML");

  PaintLayer* svgroot = GetPaintLayerByElementId("svgroot");
  EXPECT_TRUE(svgroot->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, SubsequenceCachingMuticol) {
  SetBodyInnerHTML(R"HTML(
    <div style='columns: 2'>
      <svg id='svgroot' style='position: relative'></svg>
    </div>
  )HTML");

  PaintLayer* svgroot = GetPaintLayerByElementId("svgroot");
  EXPECT_FALSE(svgroot->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, NegativeZIndexChangeToPositive) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #child { position: relative; }
    </style>
    <div id='target' style='isolation: isolate'>
      <div id='child' style='z-index: -1'></div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");

  EXPECT_TRUE(
      PaintLayerPaintOrderIterator(*target, kNegativeZOrderChildren).Next());
  EXPECT_FALSE(
      PaintLayerPaintOrderIterator(*target, kPositiveZOrderChildren).Next());

  GetDocument().getElementById("child")->setAttribute(html_names::kStyleAttr,
                                                      "z-index: 1");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(
      PaintLayerPaintOrderIterator(*target, kNegativeZOrderChildren).Next());
  EXPECT_TRUE(
      PaintLayerPaintOrderIterator(*target, kPositiveZOrderChildren).Next());
}

TEST_P(PaintLayerTest, HasVisibleDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='invisible' style='position:relative'>
      <div id='visible' style='visibility: visible; position: relative'>
      </div>
    </div>
  )HTML");
  PaintLayer* invisible = GetPaintLayerByElementId("invisible");
  PaintLayer* visible = GetPaintLayerByElementId("visible");

  EXPECT_TRUE(invisible->HasVisibleDescendant());
  EXPECT_FALSE(visible->HasVisibleDescendant());
  EXPECT_FALSE(invisible->HasNonIsolatedDescendantWithBlendMode());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative; z-index: 0'>
      <div id='child' style='transform: translateZ(1px)'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_TRUE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendantChangeStyle) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative; z-index: 0'>
      <div id='child' style='position:relative '>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  EXPECT_FALSE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());

  GetDocument().getElementById("child")->setAttribute(
      html_names::kStyleAttr, "transform: translateZ(1px)");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedDescendantNotStacking) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative;'>
      <div id='child' style='transform: translateZ(1px)'>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");

  // |child| is not a stacking child of |parent|, so it has no 3D transformed
  // descendant.
  EXPECT_FALSE(parent->Has3DTransformedDescendant());
  EXPECT_FALSE(child->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, Has3DTransformedGrandchildWithPreserve3d) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='position:relative; z-index: 0'>
      <div id='child' style='transform-style: preserve-3d'>
        <div id='grandchild' style='transform: translateZ(1px)'>
        </div>
      </div>
    </div>
  )HTML");
  PaintLayer* parent = GetPaintLayerByElementId("parent");
  PaintLayer* child = GetPaintLayerByElementId("child");
  PaintLayer* grandchild = GetPaintLayerByElementId("grandchild");

  EXPECT_TRUE(parent->Has3DTransformedDescendant());
  EXPECT_TRUE(child->Has3DTransformedDescendant());
  EXPECT_FALSE(grandchild->Has3DTransformedDescendant());
}

TEST_P(PaintLayerTest, DescendantDependentFlagsStopsAtThrottledFrames) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='transform' style='transform: translate3d(4px, 5px, 6px);'>
    </div>
    <iframe id='iframe' sandbox></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='iframeTransform'
      style='transform: translate3d(4px, 5px, 6px);'/>
  )HTML");

  // Move the child frame offscreen so it becomes available for throttling.
  auto* iframe = To<HTMLIFrameElement>(GetDocument().getElementById("iframe"));
  iframe->setAttribute(html_names::kStyleAttr, "transform: translateY(5555px)");
  UpdateAllLifecyclePhasesForTest();
  // Ensure intersection observer notifications get delivered.
  test::RunPendingTasks();
  EXPECT_FALSE(GetDocument().View()->IsHiddenForThrottling());
  EXPECT_TRUE(ChildDocument().View()->IsHiddenForThrottling());

  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(ChildDocument().View()->ShouldThrottleRenderingForTest());

  ChildDocument().View()->GetLayoutView()->Layer()->DirtyVisibleContentStatus();

  EXPECT_TRUE(ChildDocument()
                  .View()
                  ->GetLayoutView()
                  ->Layer()
                  ->needs_descendant_dependent_flags_update_);

  // Also check that the rest of the lifecycle succeeds without crashing due
  // to a stale m_needsDescendantDependentFlagsUpdate.
  UpdateAllLifecyclePhasesForTest();

  // Still dirty, because the frame was throttled.
  EXPECT_TRUE(ChildDocument()
                  .View()
                  ->GetLayoutView()
                  ->Layer()
                  ->needs_descendant_dependent_flags_update_);

  // Do an unthrottled compositing update, this should clear flags;
  GetDocument().View()->UpdateLifecycleToCompositingCleanPlusScrolling(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(ChildDocument()
                   .View()
                   ->GetLayoutView()
                   ->Layer()
                   ->needs_descendant_dependent_flags_update_);
}

TEST_P(PaintLayerTest, CompositingContainerStackedFloatUnderStackingInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9'>
          <div id='target' style='float: right; position: relative'></div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest, CompositingContainerColumnSpanAll) {
  SetBodyInnerHTML(R"HTML(
    <div>
      <div id='compositedContainer' style='columns: 1'>
        <div id='columnSpan' style='-webkit-column-span: all; overflow: hidden'>
        </div>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("columnSpan");
  EXPECT_EQ(target->Parent(), target->CompositingContainer());
  EXPECT_EQ(target->Parent()->Parent(), target->ContainingLayer());
}

TEST_P(PaintLayerTest,
       CompositingContainerStackedFloatUnderStackingCompositedInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9; will-change: transform'>
          <div id='target' style='float: right; position: relative'></div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  PaintLayer* span = GetPaintLayerByElementId("span");
  EXPECT_EQ(span, target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(span,
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest, CompositingContainerNonStackedFloatUnderStackingInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9'>
          <div id='target' style='float: right; overflow: hidden'></div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
  } else {
    EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
              target->CompositingContainer());
  }

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerNonStackedFloatUnderStackingCompositedInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9; will-change: transform'>
          <div id='target' style='float: right; overflow: hidden'></div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
  } else {
    EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
              target->CompositingContainer());
  }

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
      EXPECT_EQ(GetPaintLayerByElementId("span"),
                target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
    } else {
      EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
                target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
    }
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerStackedUnderFloatUnderStackingInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9'>
          <div style='float: right'>
            <div id='target' style='position: relative'></div>
          </div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerStackedUnderFloatUnderStackingCompositedInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9; will-change: transform'>
          <div style='float: right'>
            <div id='target' style='position: relative'></div>
          </div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  PaintLayer* span = GetPaintLayerByElementId("span");
  EXPECT_EQ(span, target->CompositingContainer());

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(span,
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerNonStackedUnderFloatUnderStackingInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9'>
          <div style='float: right'>
            <div id='target' style='overflow: hidden'></div>
          </div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
  } else {
    EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
              target->CompositingContainer());
  }

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest,
       CompositingContainerNonStackedUnderFloatUnderStackingCompositedInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <span id='span' style='opacity: 0.9; will-change: transform'>
          <div style='float: right'>
            <div id='target' style='overflow: hidden'></div>
          </div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
  } else {
    EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
              target->CompositingContainer());
  }

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
      EXPECT_EQ(GetPaintLayerByElementId("span"),
                target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
    } else {
      EXPECT_EQ(GetPaintLayerByElementId("compositedContainer"),
                target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
    }
  }
}

TEST_P(PaintLayerTest, FloatLayerAndAbsoluteUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position: absolute; top: 20px; left: 20px'>
      <div style='margin: 33px'>
        <span id='span' style='position: relative; top: 100px; left: 100px'>
          <div id='floating'
            style='float: left; position: relative; top: 50px; left: 50px'>
          </div>
          <div id='absolute'
            style='position: absolute; top: 50px; left: 50px'>
          </div>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* absolute = GetPaintLayerByElementId("absolute");
  PaintLayer* span = GetPaintLayerByElementId("span");
  PaintLayer* container = GetPaintLayerByElementId("container");

  EXPECT_EQ(span, floating->Parent());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, floating->ContainingLayer());
  } else {
    EXPECT_EQ(container, floating->ContainingLayer());
  }
  EXPECT_EQ(span, absolute->Parent());
  EXPECT_EQ(span, absolute->ContainingLayer());
  EXPECT_EQ(container, span->Parent());
  EXPECT_EQ(container, span->ContainingLayer());

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(150, 150),
              floating->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(150, 150),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(container));
  } else {
    EXPECT_EQ(PhysicalOffset(33, 33),
              floating->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(50, 50),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(-50, -50),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(83, 83),
              floating->VisualOffsetFromAncestor(container));
  }

  EXPECT_EQ(PhysicalOffset(20, 20), container->LocationWithoutPositionOffset());

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(33, 33), span->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              span->GetLayoutObject().OffsetForInFlowPosition());
  } else {
    EXPECT_EQ(PhysicalOffset(33, 33), span->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(100, 100),
              span->GetLayoutObject().OffsetForInFlowPosition());
  }

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(150, 150),
              absolute->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(150, 150),
              absolute->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              absolute->VisualOffsetFromAncestor(container));
  } else {
    EXPECT_EQ(PhysicalOffset(50, 50),
              absolute->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(50, 50), absolute->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              absolute->VisualOffsetFromAncestor(container));
  }
}

TEST_P(PaintLayerTest, FloatLayerUnderInlineLayerScrolled) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='overflow: scroll; width: 50px; height: 50px'>
      <span id='span' style='position: relative; top: 100px; left: 100px'>
        <div id='floating'
          style='float: left; position: relative; top: 50px; left: 50px'>
        </div>
      </span>
      <div style='height: 1000px'></div>
    </div>
  )HTML");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* span = GetPaintLayerByElementId("span");
  PaintLayer* container = GetPaintLayerByElementId("container");
  container->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 400), mojom::blink::ScrollType::kProgrammatic);

  EXPECT_EQ(span, floating->Parent());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, floating->ContainingLayer());
  } else {
    EXPECT_EQ(container, floating->ContainingLayer());
  }
  EXPECT_EQ(container, span->Parent());
  EXPECT_EQ(container, span->ContainingLayer());

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(0, 0), span->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(LayoutSize(0, 400),
              span->ContainingLayer()->PixelSnappedScrolledContentOffset());
    EXPECT_EQ(PhysicalOffset(150, 150),
              floating->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(150, 150),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(150, -250),
              floating->VisualOffsetFromAncestor(container));
  } else {
    EXPECT_EQ(PhysicalOffset(0, 0), span->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(100, 100),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(LayoutSize(0, 400),
              span->ContainingLayer()->PixelSnappedScrolledContentOffset());
    EXPECT_EQ(PhysicalOffset(0, 0), floating->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(50, 50),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(LayoutSize(0, 400),
              floating->ContainingLayer()->PixelSnappedScrolledContentOffset());
    EXPECT_EQ(PhysicalOffset(-50, -50),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(50, -350),
              floating->VisualOffsetFromAncestor(container));
  }
}

TEST_P(PaintLayerTest, FloatLayerUnderBlockUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <span id='span' style='position: relative; top: 100px; left: 100px'>
      <div style='display: inline-block; margin: 33px'>
        <div id='floating'
            style='float: left; position: relative; top: 50px; left: 50px'>
        </div>
      </div>
    </span>
  )HTML");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(span, floating->Parent());
  EXPECT_EQ(span, floating->ContainingLayer());

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(0, 0), span->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  } else {
    EXPECT_EQ(PhysicalOffset(33, 33),
              floating->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(50, 50),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(0, 0), span->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(100, 100),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(83, 83), floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  }
}

TEST_P(PaintLayerTest, FloatLayerUnderFloatUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <span id='span' style='position: relative; top: 100px; left: 100px'>
      <div style='float: left; margin: 33px'>
        <div id='floating'
            style='float: left; position: relative; top: 50px; left: 50px'>
        </div>
      </div>
    </span>
  )HTML");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(span, floating->Parent());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, floating->ContainingLayer());
  } else {
    EXPECT_EQ(span->Parent(), floating->ContainingLayer());
  }

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(0, 0), span->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  } else {
    EXPECT_EQ(PhysicalOffset(0, 0), span->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(100, 100),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(33, 33),
              floating->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(50, 50),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(-17, -17),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(83, 83),
              floating->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  }
}

TEST_P(PaintLayerTest, FloatLayerUnderFloatLayerUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <span id='span' style='position: relative; top: 100px; left: 100px'>
      <div id='floatingParent'
          style='float: left; position: relative; margin: 33px'>
        <div id='floating'
            style='float: left; position: relative; top: 50px; left: 50px'>
        </div>
      </div>
    </span>
  )HTML");

  PaintLayer* floating = GetPaintLayerByElementId("floating");
  PaintLayer* floating_parent = GetPaintLayerByElementId("floatingParent");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(floating_parent, floating->Parent());
  EXPECT_EQ(floating_parent, floating->ContainingLayer());
  EXPECT_EQ(span, floating_parent->Parent());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, floating_parent->ContainingLayer());
  } else {
    EXPECT_EQ(span->Parent(), floating_parent->ContainingLayer());
  }

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(50, 50),
              floating->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(133, 133),
              floating_parent->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0), span->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(0, 0),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(133, 133),
              floating_parent->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              floating->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  } else {
    EXPECT_EQ(PhysicalOffset(0, 0), floating->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(50, 50),
              floating->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(33, 33),
              floating_parent->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0), span->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(100, 100),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(100, 100),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(-17, -17),
              floating->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(-67, -67),
              floating_parent->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(83, 83),
              floating->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  }
}

TEST_P(PaintLayerTest, LayerUnderFloatUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <span id='span' style='position: relative; top: 100px; left: 100px'>
      <div style='float: left; margin: 33px'>
        <div>
          <div id='child' style='position: relative; top: 50px; left: 50px'>
          </div>
        </div>
      </div>
    </span>
  )HTML");

  PaintLayer* child = GetPaintLayerByElementId("child");
  PaintLayer* span = GetPaintLayerByElementId("span");

  EXPECT_EQ(span, child->Parent());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, child->ContainingLayer());
  } else {
    EXPECT_EQ(span->Parent(), child->ContainingLayer());
  }

  EXPECT_EQ(PhysicalOffset(0, 0), span->LocationWithoutPositionOffset());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalOffset(183, 183), child->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(0, 0),
              child->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(0, 0),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(183, 183), child->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(183, 183),
              child->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));

  } else {
    EXPECT_EQ(PhysicalOffset(33, 33), child->LocationWithoutPositionOffset());
    EXPECT_EQ(PhysicalOffset(50, 50),
              child->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(100, 100),
              span->GetLayoutObject().OffsetForInFlowPosition());
    EXPECT_EQ(PhysicalOffset(-17, -17), child->VisualOffsetFromAncestor(span));
    EXPECT_EQ(PhysicalOffset(83, 83),
              child->VisualOffsetFromAncestor(
                  GetDocument().GetLayoutView()->Layer()));
  }
}

TEST_P(PaintLayerTest, CompositingContainerFloatingIframe) {
  SetBodyInnerHTML(R"HTML(
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative; z-index: 0'>
        <div style='backface-visibility: hidden'></div>
        <span id='span'
            style='clip-path: polygon(0px 15px, 0px 54px, 100px 0px)'>
          <iframe srcdoc='foo' id='target' style='float: right'></iframe>
        </span>
      </div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");

  // A non-positioned iframe still gets a PaintLayer because PaintLayers are
  // forced for all LayoutEmbeddedContent objects. However, such PaintLayers are
  // not stacked.
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
  } else {
    EXPECT_EQ(GetPaintLayerByElementId("containingBlock"),
              target->CompositingContainer());
  }
  PaintLayer* composited_container =
      GetPaintLayerByElementId("compositedContainer");

  // enclosingLayerWithCompositedLayerMapping is not needed or applicable to
  // CAP.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(composited_container,
              target->EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
  }
}

TEST_P(PaintLayerTest, CompositingContainerSelfPaintingNonStackedFloat) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position: relative'>
      <span id='span' style='opacity: 0.9'>
        <div id='target' style='columns: 1; float: left'></div>
      </span>
    </div>
  )HTML");

  // The target layer is self-painting, but not stacked.
  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_TRUE(target->IsSelfPaintingLayer());
  EXPECT_FALSE(target->GetLayoutObject().IsStacked());

  PaintLayer* container = GetPaintLayerByElementId("container");
  PaintLayer* span = GetPaintLayerByElementId("span");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, target->ContainingLayer());
  } else {
    EXPECT_EQ(container, target->ContainingLayer());
  }
  EXPECT_EQ(span, target->CompositingContainer());
}

TEST_P(PaintLayerTest, ColumnSpanLayerUnderExtraLayerScrolled) {
  SetBodyInnerHTML(R"HTML(
    <div id='columns' style='overflow: hidden; width: 80px; height: 80px;
        columns: 2; column-gap: 0'>
      <div id='extraLayer'
          style='position: relative; top: 100px; left: 100px'>
        <div id='spanner' style='column-span: all; position: relative;
            top: 50px; left: 50px'>
        </div>
      </div>
      <div style='height: 1000px'></div>
    </div>
  )HTML");

  PaintLayer* spanner = GetPaintLayerByElementId("spanner");
  PaintLayer* extra_layer = GetPaintLayerByElementId("extraLayer");
  PaintLayer* columns = GetPaintLayerByElementId("columns");
  columns->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(200, 0), mojom::blink::ScrollType::kProgrammatic);

  EXPECT_EQ(extra_layer, spanner->Parent());
  EXPECT_EQ(columns, spanner->ContainingLayer());
  EXPECT_EQ(columns, extra_layer->Parent()->Parent());
  EXPECT_EQ(columns, extra_layer->ContainingLayer()->Parent());

  EXPECT_EQ(PhysicalOffset(0, 0), spanner->LocationWithoutPositionOffset());
  EXPECT_EQ(PhysicalOffset(50, 50),
            spanner->GetLayoutObject().OffsetForInFlowPosition());

  EXPECT_EQ(LayoutSize(200, 0),
            spanner->ContainingLayer()->PixelSnappedScrolledContentOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), extra_layer->LocationWithoutPositionOffset());
  EXPECT_EQ(PhysicalOffset(100, 100),
            extra_layer->GetLayoutObject().OffsetForInFlowPosition());
  // -60 = 2nd-column-x(40) - scroll-offset-x(200) + x-location(100)
  // 20 = y-location(100) - column-height(80)
  EXPECT_EQ(PhysicalOffset(-60, 20),
            extra_layer->VisualOffsetFromAncestor(columns));
  EXPECT_EQ(PhysicalOffset(-150, 50),
            spanner->VisualOffsetFromAncestor(columns));
}

TEST_P(PaintLayerTest, PaintLayerTransformUpdatedOnStyleTransformAnimation) {
  SetBodyInnerHTML("<div id='target' style='will-change: transform'></div>");

  LayoutObject* target_object =
      GetDocument().getElementById("target")->GetLayoutObject();
  PaintLayer* target_paint_layer =
      To<LayoutBoxModelObject>(target_object)->Layer();
  EXPECT_EQ(nullptr, target_paint_layer->Transform());

  const ComputedStyle* old_style = target_object->Style();
  scoped_refptr<ComputedStyle> new_style = ComputedStyle::Clone(*old_style);
  new_style->SetHasCurrentTransformAnimation(true);
  target_paint_layer->UpdateTransform(old_style, *new_style);

  EXPECT_NE(nullptr, target_paint_layer->Transform());
}

TEST_P(PaintLayerTest, NeedsRepaintOnSelfPaintingStatusChange) {
  SetBodyInnerHTML(R"HTML(
    <span id='span' style='opacity: 0.1'>
      <div id='target' style='overflow: hidden; float: left;
          column-width: 10px'>
      </div>
    </span>
  )HTML");

  auto* span_layer = GetPaintLayerByElementId("span");
  auto* target_element = GetDocument().getElementById("target");
  auto* target_object = target_element->GetLayoutObject();
  auto* target_layer = To<LayoutBoxModelObject>(target_object)->Layer();

  // Target layer is self painting because it is a multicol container.
  EXPECT_TRUE(target_layer->IsSelfPaintingLayer());
  EXPECT_EQ(span_layer, target_layer->CompositingContainer());
  EXPECT_FALSE(target_layer->SelfNeedsRepaint());
  EXPECT_FALSE(span_layer->SelfNeedsRepaint());

  // Removing column-width: 10px makes target layer no longer self-painting,
  // and change its compositing container. The original compositing container
  // span_layer should be marked SelfNeedsRepaint.
  target_element->setAttribute(html_names::kStyleAttr,
                               "overflow: hidden; float: left");

  UpdateAllLifecyclePhasesExceptPaint();
  // TODO(yosin): Once multicol in LayoutNG, we should remove following
  // assignments. This is because the layout tree maybe reattached. In LayoutNG
  // phase 1, layout tree is reattached because multicol forces legacy layout.
  target_object = target_element->GetLayoutObject();
  target_layer = To<LayoutBoxModelObject>(target_object)->Layer();
  EXPECT_FALSE(target_layer->IsSelfPaintingLayer());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span_layer, target_layer->CompositingContainer());
  } else {
    EXPECT_EQ(span_layer->Parent(), target_layer->CompositingContainer());
  }
  EXPECT_TRUE(target_layer->SelfNeedsRepaint());
  EXPECT_TRUE(target_layer->CompositingContainer()->SelfNeedsRepaint());
  EXPECT_TRUE(span_layer->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerTest, NeedsRepaintOnRemovingStackedLayer) {
  SetBodyInnerHTML(
      "<style>body {margin-top: 200px; backface-visibility: hidden}</style>"
      "<div id='target' style='position: absolute; top: 0'>Text</div>");

  auto* body = GetDocument().body();
  auto* body_layer = body->GetLayoutBox()->Layer();
  auto* target_element = GetDocument().getElementById("target");
  auto* target_object = target_element->GetLayoutObject();
  auto* target_layer = To<LayoutBoxModelObject>(target_object)->Layer();

  // |container| is not the CompositingContainer of |target| because |target|
  // is stacked but |container| is not a stacking context.
  EXPECT_TRUE(target_layer->GetLayoutObject().IsStacked());
  EXPECT_NE(body_layer, target_layer->CompositingContainer());
  auto* old_compositing_container = target_layer->CompositingContainer();

  body->setAttribute(html_names::kStyleAttr, "margin-top: 0");
  target_element->setAttribute(html_names::kStyleAttr, "top: 0");
  UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_FALSE(target_object->HasLayer());
  EXPECT_TRUE(body_layer->SelfNeedsRepaint());
  EXPECT_TRUE(old_compositing_container->DescendantNeedsRepaint());

  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerTest, FrameViewContentSize) {
  SetBodyInnerHTML(
      "<style> body { width: 1200px; height: 900px; margin: 0 } </style>");
  EXPECT_EQ(IntSize(800, 600), GetDocument().View()->Size());
}

TEST_P(PaintLayerTest, ReferenceClipPathWithPageZoom) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
    </style>
    <div style='width: 200px; height: 200px; background-color: blue;
                clip-path: url(#clip)' id='content'></div>
    <svg>
      <clipPath id='clip'>
        <path d='M50,50h100v100h-100z'/>
      </clipPath>
    </svg>
  )HTML");

  auto* content = GetDocument().getElementById("content");
  auto* body = GetDocument().body();

  // A hit test on the content div within the clip should hit it.
  EXPECT_EQ(content, GetDocument().ElementFromPoint(125, 75));
  EXPECT_EQ(content, GetDocument().ElementFromPoint(75, 125));

  // A hit test on the content div outside the clip should not hit it.
  EXPECT_EQ(body, GetDocument().ElementFromPoint(151, 60));
  EXPECT_EQ(body, GetDocument().ElementFromPoint(60, 151));

  // Zoom the page by 2x,
  GetDocument().GetFrame()->SetPageZoomFactor(2);

  // A hit test on the content div within the clip should hit it.
  EXPECT_EQ(content, GetDocument().ElementFromPoint(125, 75));
  EXPECT_EQ(content, GetDocument().ElementFromPoint(75, 125));

  // A hit test on the content div outside the clip should not hit it.
  EXPECT_EQ(body, GetDocument().ElementFromPoint(151, 60));
  EXPECT_EQ(body, GetDocument().ElementFromPoint(60, 151));
}

TEST_P(PaintLayerTest, FragmentedHitTest) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    div {
      break-inside: avoid-column;
      width: 50px;
      height: 50px;
      position: relative;
    }
    </style>
    <ul style="column-count: 4; position: relative">
      <div></div>
      <div id=target style=" position: relative; transform: translateY(0px);">
      </div>
    </ul>
  )HTML");

  auto* target = GetDocument().getElementById("target");
  EXPECT_EQ(target, GetDocument().ElementFromPoint(280, 30));
}

TEST_P(PaintLayerTest, SquashingOffsets) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
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

  auto* squashed = GetPaintLayerByElementId("squashed");
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());
  PhysicalOffset point;
  PaintLayer::MapPointInPaintInvalidationContainerToBacking(
      squashed->GetLayoutObject(), point);
  EXPECT_EQ(PhysicalOffset(), point);

  EXPECT_EQ(PhysicalOffset(), squashed->ComputeOffsetFromAncestor(
                                  squashed->TransformAncestorOrRoot()));

  GetDocument().View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, 25), mojom::blink::ScrollType::kUser);
  UpdateAllLifecyclePhasesForTest();

  PaintLayer::MapPointInPaintInvalidationContainerToBacking(
      squashed->GetLayoutObject(), point);
  EXPECT_EQ(PhysicalOffset(), point);

  EXPECT_EQ(PhysicalOffset(), squashed->ComputeOffsetFromAncestor(
                                  squashed->TransformAncestorOrRoot()));
}

TEST_P(PaintLayerTest, HitTestWithIgnoreClipping) {
  SetBodyInnerHTML("<div id='hit' style='width: 90px; height: 9000px;'></div>");

  HitTestRequest request(HitTestRequest::kIgnoreClipping);
  // (10, 900) is outside the viewport clip of 800x600.
  HitTestLocation location((IntPoint(10, 900)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(GetDocument().getElementById("hit"), result.InnerNode());
}

TEST_P(PaintLayerTest, HitTestWithStopNode) {
  SetBodyInnerHTML(R"HTML(
    <div id='hit' style='width: 100px; height: 100px;'>
      <div id='child' style='width:100px;height:100px'></div>
    </div>
    <div id='overlap' style='position:relative;top:-50px;width:100px;height:100px'></div>
  )HTML");
  Element* hit = GetDocument().getElementById("hit");
  Element* child = GetDocument().getElementById("child");
  Element* overlap = GetDocument().getElementById("overlap");

  // Regular hit test over 'child'
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location((PhysicalOffset(50, 25)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(child, result.InnerNode());

  // Same hit test, with stop node.
  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive,
                           hit->GetLayoutObject());
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(hit, result.InnerNode());

  // Regular hit test over 'overlap'
  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  location = HitTestLocation((PhysicalOffset(50, 75)));
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(overlap, result.InnerNode());

  // Same hit test, with stop node, should still hit 'overlap' because it's not
  // a descendant of 'hit'.
  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive,
                           hit->GetLayoutObject());
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(overlap, result.InnerNode());

  // List-based hit test with stop node
  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                               HitTestRequest::kListBased,
                           hit->GetLayoutObject());
  location = HitTestLocation((PhysicalRect(40, 15, 20, 20)));
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(1u, result.ListBasedTestResult().size());
  EXPECT_EQ(hit, *result.ListBasedTestResult().begin());
}

TEST_P(PaintLayerTest, HitTestTableWithStopNode) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .cell {
      width: 100px;
      height: 100px;
    }
    </style>
    <table id='table'>
      <tr>
        <td><div id='cell11' class='cell'></td>
        <td><div id='cell12' class='cell'></td>
      </tr>
      <tr>
        <td><div id='cell21' class='cell'></td>
        <td><div id='cell22' class='cell'></td>
      </tr>
    </table>
    )HTML");
  Element* table = GetDocument().getElementById("table");
  Element* cell11 = GetDocument().getElementById("cell11");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location((PhysicalOffset(50, 50)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(cell11, result.InnerNode());

  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive,
                           table->GetLayoutObject());
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(table, result.InnerNode());
}

TEST_P(PaintLayerTest, HitTestSVGWithStopNode) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' style='width:100px;height:100px' viewBox='0 0 100 100'>
      <circle id='circle' cx='50' cy='50' r='50' />
    </svg>
    )HTML");
  Element* svg = GetDocument().getElementById("svg");
  Element* circle = GetDocument().getElementById("circle");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location((PhysicalOffset(50, 50)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(circle, result.InnerNode());

  request = HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive,
                           svg->GetLayoutObject());
  result = HitTestResult(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(svg, result.InnerNode());
}

TEST_P(PaintLayerTest, SetNeedsRepaintSelfPaintingUnderNonSelfPainting) {
  SetHtmlInnerHTML(R"HTML(
    <span id='span' style='opacity: 0.5'>
      <div id='floating' style='float: left; overflow: hidden'>
        <div id='multicol' style='columns: 2'>A</div>
      </div>
    </span>
  )HTML");

  auto* html_layer = To<LayoutBoxModelObject>(
                         GetDocument().documentElement()->GetLayoutObject())
                         ->Layer();
  auto* span_layer = GetPaintLayerByElementId("span");
  auto* floating_layer = GetPaintLayerByElementId("floating");
  auto* multicol_layer = GetPaintLayerByElementId("multicol");
  EXPECT_FALSE(html_layer->SelfNeedsRepaint());
  EXPECT_FALSE(span_layer->SelfNeedsRepaint());
  EXPECT_FALSE(floating_layer->SelfNeedsRepaint());
  EXPECT_FALSE(multicol_layer->SelfNeedsRepaint());

  multicol_layer->SetNeedsRepaint();
  EXPECT_TRUE(html_layer->DescendantNeedsRepaint());
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    EXPECT_TRUE(span_layer->DescendantNeedsRepaint());
  else
    EXPECT_TRUE(span_layer->SelfNeedsRepaint());
  EXPECT_TRUE(floating_layer->DescendantNeedsRepaint());
  EXPECT_TRUE(multicol_layer->SelfNeedsRepaint());
}

TEST_P(PaintLayerTest, HitTestPseudoElementWithContinuation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #target::before {
        content: ' ';
        display: block;
        height: 100px
      }
    </style>
    <span id='target'></span>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(10, 10));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(target->GetPseudoElement(kPseudoIdBefore),
            result.InnerPossiblyPseudoNode());
}

TEST_P(PaintLayerTest, HitTestFirstLetterPseudoElement) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #container { height: 100px; }
      #container::first-letter { font-size: 50px; }
    </style>
    <div id='container'>
      <div>
        <span id='target'>First letter</span>
      </div>
    </div>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  Element* container = GetDocument().getElementById("container");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(10, 10));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(container->GetPseudoElement(kPseudoIdFirstLetter),
            result.InnerPossiblyPseudoNode());
}

TEST_P(PaintLayerTest, HitTestFirstLetterInBeforePseudoElement) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #container { height: 100px; }
      #container::first-letter { font-size: 50px; }
      #target::before { content: "First letter"; }
    </style>
    <div id='container'>
      <div>
        <span id='target'></span>
      </div>
    </div>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  Element* container = GetDocument().getElementById("container");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(10, 10));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(container->GetPseudoElement(kPseudoIdFirstLetter),
            result.InnerPossiblyPseudoNode());
}

TEST_P(PaintLayerTest, HitTestFloatInsideInlineBoxContainer) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #container { font: 10px/10px Ahem; width: 70px; }
      #inline-container { border: 1px solid black; }
      #target { float: right; }
    </style>
    <div id='container'>
      <span id='inline-container'>
        <a href='#' id='target'>bar</a>
        foo
      </span>
    </div>
  )HTML");
  Node* target = GetDocument().getElementById("target")->firstChild();
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(55, 5));  // At the center of "bar"
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
}

TEST_P(PaintLayerTest, HitTestFirstLetterPseudoElementDisplayContents) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #container { height: 100px; }
      #container::first-letter { font-size: 50px; }
      #target { display: contents; }
    </style>
    <div id='container'>
      <div>
        <span id='target'>First letter</span>
      </div>
    </div>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  Element* container = GetDocument().getElementById("container");
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(10, 10));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(container->GetPseudoElement(kPseudoIdFirstLetter),
            result.InnerPossiblyPseudoNode());
}

TEST_P(PaintLayerTest, HitTestOverlayResizer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      div {
        width: 200px;
        height: 200px;
      }
      body > div {
        overflow: hidden;
        resize: both;
        display: none;
      }
      #target_0 {
        position: relative;
        z-index: -1;
      }
      #target_2 {
        position: relative;
      }
      #target_3 {
        position: relative;
        z-index: 1;
      }
    </style>
    <!--
      Definitions: Nor(Normal flow paint layer), Pos(Positive paint layer),
      Neg(Negative paint layer)
    -->
    <!--0. Neg+Pos-->
    <div id="target_0" class="resize">
      <div style="position: relative"></div>
    </div>

    <!--1. Nor+Pos-->
    <div id="target_1" class="resize">
      <div style="position: relative"></div>
    </div>

    <!--2. Pos+Pos(siblings)-->
    <div id="target_2" class="resize">
      <div style="position: relative"></div>
    </div>

    <!--3. Pos+Pos(parent-child)-->
    <div id="target_3" class="resize">
      <div style="position: relative"></div>
    </div>

    <!--4. Nor+Pos+Nor-->
    <div id="target_4" class="resize">
      <div style="position: relative; z-index: 1">
        <div style="position: relative"></div>
      </div>
    </div>

    <!--5. Nor+Pos+Neg-->
    <div id="target_5" class="resize">
      <div style="position: relative; z-index: -1">
        <div style="position: relative"></div>
      </div>
    </div>
  )HTML");

  for (int i = 0; i < 6; i++) {
    Element* target_element = GetDocument().getElementById(
        AtomicString(String::Format("target_%d", i)));
    target_element->setAttribute(html_names::kStyleAttr, "display: block");
    UpdateAllLifecyclePhasesForTest();

    HitTestRequest request(HitTestRequest::kIgnoreClipping);
    HitTestLocation location((IntPoint(198, 198)));
    HitTestResult result(request, location);
    GetDocument().GetLayoutView()->HitTest(location, result);
    if (i == 0)
      EXPECT_NE(target_element, result.InnerNode());
    else
      EXPECT_EQ(target_element, result.InnerNode());

    target_element->setAttribute(html_names::kStyleAttr, "display: none");
  }
}

TEST_P(PaintLayerTest, BackgroundIsKnownToBeOpaqueInRectChildren) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
        position: relative;
        isolation: isolate;
      }
    </style>
    <div id='target'>
      <div style='background: blue'></div>
    </div>
  )HTML");

  PaintLayer* target_layer = GetPaintLayerByElementId("target");
  EXPECT_TRUE(target_layer->BackgroundIsKnownToBeOpaqueInRect(
      PhysicalRect(0, 0, 100, 100), true));
  EXPECT_FALSE(target_layer->BackgroundIsKnownToBeOpaqueInRect(
      PhysicalRect(0, 0, 100, 100), false));
}

TEST_P(PaintLayerTest,
       ChangeAlphaNeedsCompositingInputsAndPaintPropertyUpdate) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        background: white;
        width: 100px;
        height: 100px;
        position: relative;
      }
    </style>
    <div id='target'>
    </div>
  )HTML");
  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_FALSE(target->NeedsCompositingInputsUpdate());
  EXPECT_FALSE(target->GetLayoutObject().NeedsPaintPropertyUpdate());
  EXPECT_FALSE(target->Parent()->GetLayoutObject().NeedsPaintPropertyUpdate());

  StyleDifference diff;
  diff.SetHasAlphaChanged();
  target->StyleDidChange(diff, target->GetLayoutObject().Style());
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    EXPECT_FALSE(target->NeedsCompositingInputsUpdate());
  else
    EXPECT_TRUE(target->NeedsCompositingInputsUpdate());
  EXPECT_TRUE(target->GetLayoutObject().NeedsPaintPropertyUpdate());
  // See the TODO in PaintLayer::SetNeedsCompositingInputsUpdate().
  EXPECT_TRUE(target->Parent()->GetLayoutObject().NeedsPaintPropertyUpdate());
}

TEST_P(PaintLayerTest, PaintLayerCommonAncestor) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        position: relative;
      }
    </style>
    <div id='wrapper'>
      <div id='target1'>
        <div id='target1x1'></div>
      </div>
      <div id='target2'></div>
      <div>
        <div id='target3'></div>
      </div>
    </div>
  )HTML");

  PaintLayer* wrapper = GetPaintLayerByElementId("wrapper");
  PaintLayer* target1 = GetPaintLayerByElementId("target1");
  PaintLayer* target1x1 = GetPaintLayerByElementId("target1x1");
  PaintLayer* target2 = GetPaintLayerByElementId("target2");
  PaintLayer* target3 = GetPaintLayerByElementId("target3");

  EXPECT_EQ(target1->CommonAncestor(target1), target1);
  EXPECT_EQ(target1->CommonAncestor(target1x1), target1);
  EXPECT_EQ(target1->CommonAncestor(target2), wrapper);
  EXPECT_EQ(target1->CommonAncestor(target3), wrapper);

  EXPECT_EQ(target1x1->CommonAncestor(target1), target1);
  EXPECT_EQ(target1x1->CommonAncestor(target1x1), target1x1);
  EXPECT_EQ(target1x1->CommonAncestor(target2), wrapper);
  EXPECT_EQ(target1x1->CommonAncestor(target3), wrapper);

  EXPECT_EQ(target2->CommonAncestor(target1), wrapper);
  EXPECT_EQ(target2->CommonAncestor(target1x1), wrapper);
  EXPECT_EQ(target2->CommonAncestor(target2), target2);
  EXPECT_EQ(target2->CommonAncestor(target3), wrapper);

  EXPECT_EQ(target3->CommonAncestor(target1), wrapper);
  EXPECT_EQ(target3->CommonAncestor(target1x1), wrapper);
  EXPECT_EQ(target3->CommonAncestor(target2), wrapper);
  EXPECT_EQ(target3->CommonAncestor(target3), target3);
}

TEST_P(PaintLayerTest, PaintLayerCommonAncestorBody) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body, div {
        position: relative;
      }
    </style>
    <div id='target1'></div>
    <div id='target2'></div>
  )HTML");

  PaintLayer* target1 = GetPaintLayerByElementId("target1");
  PaintLayer* target2 = GetPaintLayerByElementId("target2");

  EXPECT_EQ(target1->CommonAncestor(target2)->GetLayoutObject(),
            GetDocument().body()->GetLayoutObject());
}

TEST_P(PaintLayerTest, InlineWithBackdropFilterHasPaintLayer) {
  SetBodyInnerHTML(
      "<map id='target' style='backdrop-filter: invert(1);'></map>");
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  PaintLayer* root_layer = GetLayoutView().Layer();

  EXPECT_NE(nullptr, root_layer);
  EXPECT_NE(nullptr, paint_layer);
}

TEST_P(PaintLayerTest, DirectCompositingReasonsCrossingFrameBoundaries) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <iframe></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id=target style="position: relative"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  PaintLayer* target =
      To<LayoutBoxModelObject>(
          ChildDocument().getElementById("target")->GetLayoutObject())
          ->Layer();

  EXPECT_EQ(
      GetDocument().View()->GetLayoutView()->Layer(),
      target->EnclosingDirectlyCompositableLayerCrossingFrameBoundaries());
}

TEST_P(PaintLayerTest,
       DirectCompositingReasonsCrossingFrameBoundariesCompositedIframe) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <iframe id="iframe" style="will-change: transform";></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id=target style="position: relative"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  PaintLayer* target =
      To<LayoutBoxModelObject>(
          ChildDocument().getElementById("target")->GetLayoutObject())
          ->Layer();

  PaintLayer* iframe =
      To<LayoutBoxModelObject>(
          GetDocument().getElementById("iframe")->GetLayoutObject())
          ->Layer();

  EXPECT_EQ(
      iframe,
      target->EnclosingDirectlyCompositableLayerCrossingFrameBoundaries());
}

TEST_P(PaintLayerTest,
       DirectCompositingReasonsCrossingFrameBoundariesCompositedParent) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <iframe></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id="parent" style="will-change: transform">
      <div id=target style="position: relative"></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  PaintLayer* target =
      To<LayoutBoxModelObject>(
          ChildDocument().getElementById("target")->GetLayoutObject())
          ->Layer();

  PaintLayer* parent =
      To<LayoutBoxModelObject>(
          ChildDocument().getElementById("parent")->GetLayoutObject())
          ->Layer();

  EXPECT_EQ(
      parent,
      target->EnclosingDirectlyCompositableLayerCrossingFrameBoundaries());
}

TEST_P(PaintLayerTest, GlobalRootScrollerHitTest) {
  SetBodyInnerHTML(R"HTML(
    <style>
      :root {
        clip-path: circle(30%);
        background:blue;
        transform: rotate(30deg);
        transform-style: preserve-3d;
      }
      #perspective {
        perspective:100px;
      }
      #threedee {
        transform: rotate3d(1, 1, 1, 45deg);
        width:100px; height:200px;
      }
    </style>
    <div id="perspective">
      <div id="threedee"></div>
    </div>
  )HTML");
  GetDocument().GetPage()->SetPageScaleFactor(2);
  UpdateAllLifecyclePhasesForTest();

  const HitTestRequest hit_request(HitTestRequest::kActive);
  const HitTestLocation location(IntPoint(400, 300));
  HitTestResult result;
  GetLayoutView().HitTestNoLifecycleUpdate(location, result);
  EXPECT_EQ(result.InnerNode(), GetDocument().documentElement());
  EXPECT_EQ(result.GetScrollbar(), nullptr);

  if (GetDocument().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    const HitTestLocation location_scrollbar(IntPoint(790, 300));
    HitTestResult result_scrollbar;
    EXPECT_EQ(result_scrollbar.InnerNode(), &GetDocument());
    EXPECT_NE(result_scrollbar.GetScrollbar(), nullptr);
  }
}

TEST_P(PaintLayerTest, HasNonEmptyChildLayoutObjectsZeroSizeOverflowVisible) {
  SetBodyInnerHTML(R"HTML(
    <div id="layer" style="position: relative">
      <div style="overflow: visible; height: 0; width: 0">text</div>
    </div>
  )HTML");

  auto* layer = GetPaintLayerByElementId("layer");
  EXPECT_TRUE(layer->HasVisibleContent());
  EXPECT_FALSE(layer->HasVisibleDescendant());
  EXPECT_TRUE(layer->HasNonEmptyChildLayoutObjects());
}

enum { kCompositingOptimizations = 1 << 0 };

// TODO(chrishtr): Remove this test configuration and keep the appropriate
// variants of the tests when CompositingOptimizations ships or is removed.
class PaintLayerOverlapTestConfigurations
    : public testing::WithParamInterface<unsigned>,
      private ScopedCompositingOptimizationsForTest {
 public:
  PaintLayerOverlapTestConfigurations()
      : ScopedCompositingOptimizationsForTest(GetParam() &
                                              kCompositingOptimizations) {}
  ~PaintLayerOverlapTestConfigurations() override {
    // Must destruct all objects before toggling back feature flags.
    WebHeap::CollectAllGarbageForTesting();
  }
};

class PaintLayerOverlapTest : public PaintLayerOverlapTestConfigurations,
                              public RenderingTest {
 public:
  PaintLayerOverlapTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PaintLayerOverlapTest,
                         ::testing::Values(0, kCompositingOptimizations));

TEST_P(PaintLayerOverlapTest, FixedUsesExpandedBoundingBoxForOverlap) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      body {
        height: 610px;
        width: 820px;
      }
      #fixed {
        height: 10px;
        left: 50px;
        position: fixed;
        top: 50px;
        width: 10px;
      }
    </style>
    <div id=fixed></div>
  )HTML");

  PaintLayer* fixed = GetPaintLayerByElementId("fixed");
  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(50, 50, 30, 20));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 10, 10));
  EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
  EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));

  // Modify the viewport scroll offset and ensure that the bounding box is still
  // adjusted by the new amount the viewport can scroll in any direction.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(10, 10), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(50, 50, 30, 20));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 10, 10));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
  } else {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(60, 60, 10, 10));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(60, 60, 10, 10));
  }
}

TEST_P(PaintLayerOverlapTest,
       FixedInScrollerUsesExpandedBoundingBoxForOverlap) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      body {
        height: 610px;
        width: 820px;
      }
      #scroller {
        height: 100px;
        left: 100px;
        overflow: scroll;
        position: absolute;
        top: 100px;
        width: 100px;
      }
      #spacer {
        height: 500px;
      }
      #fixed {
        height: 10px;
        left: 50px;
        position: fixed;
        top: 50px;
        width: 10px;
      }
    </style>
    <div id=scroller>
      <div id=fixed></div>
      <div id=spacer></div>
    </div>
  )HTML");

  PaintLayer* fixed = GetPaintLayerByElementId("fixed");
  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(50, 50, 30, 20));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 10, 10));
  EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
  EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));

  // Modify the inner scroll offset and ensure that the bounding box is still
  // the same.
  PaintLayerScrollableArea* scrollable_area =
      GetPaintLayerByElementId("scroller")->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(FloatPoint(10, 10));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(50, 50, 30, 20));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 10, 10));
  EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
  EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));

  // Modify the viewport scroll offset and ensure that the bounding box is still
  // adjusted by the newamount the viewport can scroll in any direction.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(10, 10), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(50, 50, 30, 20));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 10, 10));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
  } else {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(60, 60, 10, 10));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(60, 60, 10, 10));
  }
}

TEST_P(PaintLayerOverlapTest,
       FixedUnderTransformDoesNotExpandBoundingBoxForOverlap) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .anim {
        animation: pulse 5s infinite;
      }
      @keyframes pulse {
        0% { opacity: 0.1; }
        100% { opacity: 0.9; }
      }
      .xform {
        height: 100px;
        left: 100px;
        position: absolute;
        top: 100px;
        transform: rotate(20deg);
        width: 100px;
      }
      .fixed {
        height: 50px;
        left: 25px;
        position: fixed;
        top: 25px;
        width: 50px;
      }
      .spacer {
        height: 2000px;
      }
    </style>
    <div id=fixed-cb class=xform>
      <div id=fixed class='fixed anim'></div>
    </div>
    <div class=spacer></div>
  )HTML");

  // The animation is to cause the fixed to be composited. However, even with
  // fixed composited, it shouldn't have expanded bounds because its containing
  // block isn't the viewport.
  PaintLayer* fixed = GetPaintLayerByElementId("fixed");
  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(117, 117, 66, 66));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(117, 117, 66, 66));
  EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(117, 117, 66, 66));
}

TEST_P(PaintLayerOverlapTest, NestedFixedUsesExpandedBoundingBoxForOverlap) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      body {
        height: 610px;
        width: 820px;
      }
      #iframe1 {
        height: 100px;
        left: 50px;
        position: fixed;
        top: 50px;
        width: 100px;
      }
    </style>
    <iframe id=iframe1></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      body {
        height: 500px;
        width: 500px;
      }
      #fixed {
        height: 10px;
        left: 50px;
        position: fixed;
        top: 50px;
        width: 10px;
      }
    </style>
    <div id=fixed></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  PaintLayer* fixed =
      To<LayoutBoxModelObject>(
          ChildDocument().getElementById("fixed")->GetLayoutObject())
          ->Layer();
  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(50, 50, 410, 410));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 10, 10));
  EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
  EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));

  // Modify the top-most viewport's scroll offset and ensure that the bounding
  // box is still the same. This shows that we're not considering the wrong
  // viewport's scroll offset when computing the bounding box.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(10, 10), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(50, 50, 410, 410));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 10, 10));
  EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
  EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));

  // Now modify the iframe's scroll offset. This one should affect the fixed's
  // bounding box.
  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(10, 10), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(50, 50, 410, 410));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 10, 10));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(50, 50, 10, 10));
  } else {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(60, 60, 10, 10));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(60, 60, 10, 10));
  }
}

TEST_P(PaintLayerOverlapTest, FixedWithExpandedBoundsForChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      body {
        height: 1000px;
      }
      #fixed {
        height: 50px;
        left: 25px;
        position: fixed;
        top: 25px;
        width: 50px;
      }
      #abs {
        height: 25px;
        left: 50px;
        position: absolute;
        top: 200px;
        width: 25px;
        will-change: transform;
      }
    </style>
    <div id=fixed>
      <div id=abs></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  // The fixed-pos layer should use bounds that have been expanded to include
  // the absolutely positioned child. Without this expansion, overlap testing
  // can miss overlap from that child leading to incorrect composition order.
  PaintLayer* fixed = GetPaintLayerByElementId("fixed");
  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 75, 625));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));

  // Verify that the abs bounds is not expanded even though it is a child of a
  // fixed-pos layer. Expanding the abs bounds would mean that it could
  // unnecessarily detect overlap with siblings that it doesn't ever actually
  // overlap with.
  PaintLayer* abs = GetPaintLayerByElementId("abs");
  EXPECT_EQ(abs->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(75, 225, 25, 25));
  EXPECT_EQ(abs->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
  EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));

  // Modify the scroll offset and ensure that the bounding box is still the
  // same. Note that if we get different expanded bounding boxes for overlap
  // testing with different scroll offsets then it implies that scroll offset is
  // a part of that calculation and we may get incorrect results as scroll
  // offsets changes and partial updates happen.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 400), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 75, 625));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  } else {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
  }

  EXPECT_EQ(abs->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(75, 625, 25, 25));
  EXPECT_EQ(abs->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
  } else {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
  }
}

TEST_P(PaintLayerOverlapTest, FixedWithClippedExpandedBoundsForChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      body {
        height: 1000px;
      }
      #fixed {
        height: 50px;
        left: 25px;
        position: fixed;
        top: 25px;
        width: 50px;
        overflow: hidden;
      }
      #abs {
        height: 25px;
        left: 50px;
        position: absolute;
        top: 200px;
        width: 25px;
        will-change: transform;
      }
    </style>
    <div id=fixed>
      <div id=abs></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  // The fixed-pos layer should use bounds that have been expanded to include
  // the absolutely positioned child. However, the fixed-pos ancestor also has
  // clipping which will limit the expansion.
  PaintLayer* fixed = GetPaintLayerByElementId("fixed");
  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 50, 450));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));

  // Verify that the abs bounds is not expanded even though it is a child of a
  // fixed-pos layer. Expanding the abs bounds would mean that it could
  // unnecessarily detect overlap with siblings that it doesn't ever actually
  // overlap with. Note that the clipped bounds is an empty rect because of the
  // clipping from the ancestor.
  PaintLayer* abs = GetPaintLayerByElementId("abs");
  EXPECT_EQ(abs->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(75, 225, 25, 25));
  EXPECT_EQ(abs->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
  EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(0, 0, 0, 0));

  // Modify the scroll offset and ensure that the bounding box is still the
  // same. Note that if we get different expanded bounding boxes for overlap
  // testing with different scroll offsets then it implies that scroll offset is
  // a part of that calculation and we may get incorrect results as scroll
  // offsets changes and partial updates happen.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 400), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 50, 450));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  } else {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
  }

  EXPECT_EQ(abs->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(75, 625, 25, 25));
  EXPECT_EQ(abs->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(0, 0, 0, 0));
  } else {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(0, 0, 0, 0));
  }
}

TEST_P(PaintLayerOverlapTest, FixedWithExpandedBoundsForGrandChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      body {
        height: 1000px;
      }
      #fixed {
        height: 50px;
        left: 25px;
        position: fixed;
        top: 25px;
        width: 50px;
      }
      #abs {
        height: 25px;
        left: 50px;
        position: absolute;
        top: 200px;
        width: 25px;
      }
      #abs2 {
        height: 25px;
        left: 50px;
        position: absolute;
        top: 100px;
        width: 25px;
      }
    </style>
    <div id=fixed>
      <div id=abs>
        <div id=abs2></div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  // The fixed-pos layer should use bounds that have been expanded to include
  // the absolutely positioned grandchild.
  PaintLayer* fixed = GetPaintLayerByElementId("fixed");
  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 125, 725));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));

  // Verify that the abs bounds is not expanded even though it is a child of a
  // fixed-pos layer. Additionally, it shouldn't include its child as only
  // fixed-pos expands to include descendants.
  PaintLayer* abs = GetPaintLayerByElementId("abs");
  EXPECT_EQ(abs->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(75, 225, 25, 25));
  EXPECT_EQ(abs->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
  EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));

  PaintLayer* abs2 = GetPaintLayerByElementId("abs2");
  EXPECT_EQ(abs2->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(125, 325, 25, 25));
  EXPECT_EQ(abs2->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  EXPECT_EQ(abs2->UnclippedAbsoluteBoundingBox(), IntRect(125, 325, 25, 25));
  EXPECT_EQ(abs2->ClippedAbsoluteBoundingBox(), IntRect(125, 325, 25, 25));

  // Modify the scroll offset and ensure that the bounding box is still the
  // same. Note that if we get different expanded bounding boxes for overlap
  // testing with different scroll offsets then it implies that scroll offset is
  // a part of that calculation and we may get incorrect results as scroll
  // offsets changes and partial updates happen.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 400), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 125, 725));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  } else {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
  }

  EXPECT_EQ(abs->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(75, 625, 25, 25));
  EXPECT_EQ(abs->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
  } else {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
  }

  EXPECT_EQ(abs2->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(125, 725, 25, 25));
  EXPECT_EQ(abs2->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(abs2->UnclippedAbsoluteBoundingBox(), IntRect(125, 325, 25, 25));
    EXPECT_EQ(abs2->ClippedAbsoluteBoundingBox(), IntRect(125, 325, 25, 25));
  } else {
    EXPECT_EQ(abs2->UnclippedAbsoluteBoundingBox(), IntRect(125, 725, 25, 25));
    EXPECT_EQ(abs2->ClippedAbsoluteBoundingBox(), IntRect(125, 725, 25, 25));
  }

  // Add will-change to the middle child to ensure the bounds are still the
  // same. This helps confirm that the computation of the bounds is agnostic to
  // if descendants are composited or not.
  GetDocument().getElementById("abs")->setAttribute(html_names::kStyleAttr,
                                                    "will-change: transform");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 125, 725));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  } else {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
  }

  EXPECT_EQ(abs->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(75, 625, 25, 25));
  EXPECT_EQ(abs->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
  } else {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
  }

  EXPECT_EQ(abs2->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(125, 725, 25, 25));
  EXPECT_EQ(abs2->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(abs2->UnclippedAbsoluteBoundingBox(), IntRect(125, 325, 25, 25));
    EXPECT_EQ(abs2->ClippedAbsoluteBoundingBox(), IntRect(125, 325, 25, 25));
  } else {
    EXPECT_EQ(abs2->UnclippedAbsoluteBoundingBox(), IntRect(125, 725, 25, 25));
    EXPECT_EQ(abs2->ClippedAbsoluteBoundingBox(), IntRect(125, 725, 25, 25));
  }

  // Add will-change to the grandchild and ensure the bounds are still the same.
  GetDocument().getElementById("abs2")->setAttribute(html_names::kStyleAttr,
                                                     "will-change: transform");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 125, 725));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  } else {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
  }

  EXPECT_EQ(abs->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(75, 625, 25, 25));
  EXPECT_EQ(abs->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
  } else {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
  }

  EXPECT_EQ(abs2->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(125, 725, 25, 25));
  EXPECT_EQ(abs2->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(abs2->UnclippedAbsoluteBoundingBox(), IntRect(125, 325, 25, 25));
    EXPECT_EQ(abs2->ClippedAbsoluteBoundingBox(), IntRect(125, 325, 25, 25));
  } else {
    EXPECT_EQ(abs2->UnclippedAbsoluteBoundingBox(), IntRect(125, 725, 25, 25));
    EXPECT_EQ(abs2->ClippedAbsoluteBoundingBox(), IntRect(125, 725, 25, 25));
  }

  // Remove will-change from the middle child and ensure the bounds are still
  // the same.
  GetDocument().getElementById("abs")->setAttribute(html_names::kStyleAttr, "");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 125, 725));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  } else {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
  }

  EXPECT_EQ(abs->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(75, 625, 25, 25));
  EXPECT_EQ(abs->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 225, 25, 25));
  } else {
    EXPECT_EQ(abs->UnclippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
    EXPECT_EQ(abs->ClippedAbsoluteBoundingBox(), IntRect(75, 625, 25, 25));
  }

  EXPECT_EQ(abs2->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(125, 725, 25, 25));
  EXPECT_EQ(abs2->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(abs2->UnclippedAbsoluteBoundingBox(), IntRect(125, 325, 25, 25));
    EXPECT_EQ(abs2->ClippedAbsoluteBoundingBox(), IntRect(125, 325, 25, 25));
  } else {
    EXPECT_EQ(abs2->UnclippedAbsoluteBoundingBox(), IntRect(125, 725, 25, 25));
    EXPECT_EQ(abs2->ClippedAbsoluteBoundingBox(), IntRect(125, 725, 25, 25));
  }
}

TEST_P(PaintLayerOverlapTest, FixedWithExpandedBoundsForFixedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      body {
        height: 1000px;
      }
      #fixed {
        height: 50px;
        left: 25px;
        position: fixed;
        top: 25px;
        width: 50px;
      }
      #nestedFixed {
        height: 25px;
        left: 50px;
        position: fixed;
        top: 100px;
        width: 25px;
      }
    </style>
    <div id=fixed>
      <div id=nestedFixed></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  // The fixed-pos layer should use bounds that have been expanded to include
  // the absolutely positioned child. Without this expansion, overlap testing
  // can miss overlap from that child leading to incorrect composition order.
  PaintLayer* fixed = GetPaintLayerByElementId("fixed");
  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 50, 500));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));

  // Note that the nested fixed should not expand its bounds as it doesn't move
  // relative to its siblings, fixed-pos or not.
  PaintLayer* nestedFixed = GetPaintLayerByElementId("nestedFixed");
  EXPECT_EQ(nestedFixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(50, 100, 25, 25));
  EXPECT_EQ(nestedFixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  EXPECT_EQ(nestedFixed->UnclippedAbsoluteBoundingBox(),
            IntRect(50, 100, 25, 25));
  EXPECT_EQ(nestedFixed->ClippedAbsoluteBoundingBox(),
            IntRect(50, 100, 25, 25));

  // Modify the scroll offset and ensure that the bounding box is still the
  // same.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 400), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(fixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(25, 25, 50, 500));
  EXPECT_EQ(fixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 50, 50));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 25, 50, 50));
  } else {
    EXPECT_EQ(fixed->UnclippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
    EXPECT_EQ(fixed->ClippedAbsoluteBoundingBox(), IntRect(25, 425, 50, 50));
  }

  EXPECT_EQ(nestedFixed->ExpandedBoundingBoxForCompositingOverlapTest(false),
            IntRect(50, 500, 25, 25));
  EXPECT_EQ(nestedFixed->LocalBoundingBoxForCompositingOverlapTest(),
            PhysicalRect(0, 0, 25, 25));
  if (!RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    EXPECT_EQ(nestedFixed->UnclippedAbsoluteBoundingBox(),
              IntRect(50, 100, 25, 25));
    EXPECT_EQ(nestedFixed->ClippedAbsoluteBoundingBox(),
              IntRect(50, 100, 25, 25));
  } else {
    EXPECT_EQ(nestedFixed->UnclippedAbsoluteBoundingBox(),
              IntRect(50, 500, 25, 25));
    EXPECT_EQ(nestedFixed->ClippedAbsoluteBoundingBox(),
              IntRect(50, 500, 25, 25));
  }
}

}  // namespace blink
