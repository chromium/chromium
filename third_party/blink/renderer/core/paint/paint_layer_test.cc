// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
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

TEST_P(PaintLayerTest, RootLayerScrollBounds) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetBodyInnerHTML(
      "<style> body { width: 1000px; height: 1000px; margin: 0 } </style>");
  PaintLayerScrollableArea* plsa = GetLayoutView().Layer()->GetScrollableArea();

  int scrollbarThickness = plsa->VerticalScrollbarWidth();
  EXPECT_EQ(scrollbarThickness, plsa->HorizontalScrollbarHeight());
  EXPECT_GT(scrollbarThickness, 0);

  EXPECT_EQ(ScrollOffset(200 + scrollbarThickness, 400 + scrollbarThickness),
            plsa->MaximumScrollOffset());

  EXPECT_EQ(gfx::Rect(0, 0, 800 - scrollbarThickness, 600 - scrollbarThickness),
            plsa->VisibleContentRect());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600),
            plsa->VisibleContentRect(kIncludeScrollbars));
}

TEST_P(PaintLayerTest, CompositedScrollingNoNeedsRepaint) {
  SetBodyInnerHTML(R"HTML(
    <div id='scroll' style='width: 100px; height: 100px; overflow: scroll;
        will-change: transform'>
      <div id='content' style='position: relative; background: blue;
          width: 2000px; height: 2000px'></div>
    </div>
  )HTML");

  PaintLayer* scroll_layer = GetPaintLayerByElementId("scroll");

  PaintLayer* content_layer = GetPaintLayerByElementId("content");

  scroll_layer->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(1000, 1000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(
      gfx::Vector2d(1000, 1000),
      content_layer->ContainingLayer()->PixelSnappedScrolledContentOffset());
  EXPECT_FALSE(content_layer->SelfNeedsRepaint());
  EXPECT_FALSE(scroll_layer->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerTest, NonCompositedScrollingNeedsRepaint) {
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
  EXPECT_FALSE(scroll_layer->GetLayoutObject()
                   .FirstFragment()
                   .PaintProperties()
                   ->ScrollTranslation()
                   ->HasDirectCompositingReasons());

  PaintLayer* content_layer = GetPaintLayerByElementId("content");
  const auto& fragment = content_layer->GetLayoutObject().FirstFragment();
  EXPECT_EQ(gfx::Rect(0, 0, 2000, 2000), fragment.GetContentsCullRect().Rect());

  scroll_layer->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(1000, 1000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(
      gfx::Vector2d(1000, 1000),
      content_layer->ContainingLayer()->PixelSnappedScrolledContentOffset());

  EXPECT_FALSE(scroll_layer->SelfNeedsRepaint());
  EXPECT_EQ(gfx::Rect(0, 0, 2000, 2000), fragment.GetContentsCullRect().Rect());
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
  EXPECT_TRUE(parent->HasVisibleSelfPaintingDescendant());
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

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("position: relative"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(parent->HasFixedPositionDescendant());
  EXPECT_FALSE(child->HasFixedPositionDescendant());
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

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("position: absolute"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(parent->HasNonContainedAbsolutePositionDescendant());
  EXPECT_FALSE(child->HasNonContainedAbsolutePositionDescendant());

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("position: relative"));
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

static const HeapVector<Member<PaintLayer>>*
LayersPaintingOverlayOverflowControlsAfter(const PaintLayer* layer) {
  return PaintLayerPaintOrderIterator(layer->AncestorStackingContext(),
                                      kPositiveZOrderChildren)
      .LayersPaintingOverlayOverflowControlsAfter(layer);
}

// We need new enum and class to test the overlay overflow controls reordering,
// but we don't move the tests related to the new class to the bottom, which is
// behind all tests of the PaintLayerTest. Because it will make the git history
// hard to track.
enum OverlayType { kOverlayResizer, kOverlayScrollbars };

class ReorderOverlayOverflowControlsTest
    : public testing::WithParamInterface<OverlayType>,
      public PaintControllerPaintTestBase {
 public:
  ReorderOverlayOverflowControlsTest()
      : PaintControllerPaintTestBase(
            MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
  ~ReorderOverlayOverflowControlsTest() override {
    // Must destruct all objects before toggling back feature flags.
    WebHeap::CollectAllGarbageForTesting();
  }

  OverlayType GetOverlayType() const { return GetParam(); }

  void InitOverflowStyle(const char* id) {
    auto* element = GetElementById(id);
    element->setAttribute(html_names::kStyleAttr,
                          AtomicString(GetOverlayType() == kOverlayScrollbars
                                           ? "overflow: auto"
                                           : "overflow: hidden; resize: both"));
    UpdateAllLifecyclePhasesForTest();
    if (GetOverlayType() == kOverlayScrollbars) {
      element->GetLayoutBox()
          ->GetScrollableArea()
          ->SetScrollbarsHiddenIfOverlay(false);
      UpdateAllLifecyclePhasesForTest();
    }
  }

  void RemoveOverflowStyle(const char* id) {
    GetElementById(id)->setAttribute(html_names::kStyleAttr,
                                     AtomicString("overflow: visible"));
    UpdateAllLifecyclePhasesForTest();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ReorderOverlayOverflowControlsTest,
                         ::testing::Values(kOverlayScrollbars,
                                           kOverlayResizer));

TEST_P(ReorderOverlayOverflowControlsTest, StackedWithInFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
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
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("position: relative; height: 80px"));
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
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(
          html_names::kStyleAttr,
          AtomicString("position: relative; width: 200px; height: 80px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("width: 200px; height: 80px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(
          html_names::kStyleAttr,
          AtomicString("position: relative; width: 200px; height: 80px"));
  UpdateAllLifecyclePhasesForTest();
  child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest, StackedWithOutOfFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #parent {
        position: relative;
        width: 100px;
        height: 100px;
      }
      #child {
        width: 200px;
        height: 200px;
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
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("position: absolute"));
  UpdateAllLifecyclePhasesForTest();
  child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest, StackedWithZIndexDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #parent {
        position: relative;
        width: 100px;
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
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("z-index: -1"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("z-index: 2"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       NestedStackedWithInFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #ancestor {
        position: relative;
        width: 100px;
        height: 100px;
      }
      #parent {
        width: 100px;
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
  EXPECT_EQ(ancestor->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       NestedStackedWithOutOfFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #ancestor {
        position: relative;
        width: 100px;
        height: 100px;
      }
      #parent {
        position: absolute;
        width: 100px;
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
  EXPECT_EQ(ancestor->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest, MultipleChildren) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
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
        top: 0;
        z-index: 1;
      }
      #middle-child {
        position: relative;
        z-index: 2;
      }
      #high-child {
        position: absolute;
        top: 0;
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
  EXPECT_EQ(high_child->GetLayoutObject().GetNode(), HitTest(99, 99));

  std::string extra_style = GetOverlayType() == kOverlayScrollbars
                                ? "overflow: auto;"
                                : "overflow: hidden; resize: both;";
  std::string new_style = extra_style + "position: absolute; z-index: 1";
  GetDocument()
      .getElementById(AtomicString("parent"))
      ->setAttribute(html_names::kStyleAttr, AtomicString(new_style.c_str()));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(low_child));
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(middle_child));
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(high_child));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  new_style = extra_style + "position: absolute;";
  GetDocument()
      .getElementById(AtomicString("parent"))
      ->setAttribute(html_names::kStyleAttr, AtomicString(new_style.c_str()));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(low_child));
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(middle_child));
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(high_child),
              Pointee(ElementsAre(parent)));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest, NonStackedWithInFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin : 0; }
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
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("position: relative; height: 80px"));
  UpdateAllLifecyclePhasesForTest();
  if (GetOverlayType() == kOverlayResizer) {
    EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
    EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
                Pointee(ElementsAre(parent)));
  } else {
    EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
    EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));
  }
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(
          html_names::kStyleAttr,
          AtomicString("position: relative; width: 200px; height: 80px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("width: 200px; height: 80px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(
          html_names::kStyleAttr,
          AtomicString("position: relative; width: 200px; height: 80px"));
  UpdateAllLifecyclePhasesForTest();
  child = GetPaintLayerByElementId("child");
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       NonStackedWithZIndexInFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #parent {
        width: 100px;
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
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("z-index: -1"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("z-index: 2"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest, NonStackedWithOutOfFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #parent {
        width: 100px;
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
  EXPECT_EQ(child->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest, NonStackedWithNonStackedDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #parent {
        width: 100px;
        height: 100px;
      }
      #child {
        width: 200px;
        height: 200px;
      }
    </style>
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
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       NestedNonStackedWithInFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #ancestor {
        width: 100px;
        height: 100px;
      }
      #parent {
        width: 100px;
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
  EXPECT_EQ(ancestor->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       NestedNonStackedWithOutOfFlowStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #ancestor {
        width: 100px;
        height: 100px;
      }
      #parent {
        width: 100px;
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
  EXPECT_EQ(child->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest,
       AdjustAccessingOrderForSubtreeHighestLayers) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      div {
        width: 200px;
        height: 200px;
      }
      div > div {
        height: 300px;
      }
      #ancestor {
        width: 100px;
        height: 100px;
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
  EXPECT_EQ(ancestor->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest, AddRemoveScrollableArea) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #parent {
        position: relative;
        width: 100px;
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

  auto* parent = GetPaintLayerByElementId("parent");
  auto* child = GetPaintLayerByElementId("child");
  EXPECT_FALSE(parent->GetScrollableArea());
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));
  EXPECT_EQ(child->GetLayoutObject().GetNode(), HitTest(99, 99));

  InitOverflowStyle("parent");
  EXPECT_TRUE(parent->GetScrollableArea());
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(child),
              Pointee(ElementsAre(parent)));
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  RemoveOverflowStyle("parent");
  EXPECT_FALSE(parent->GetScrollableArea());
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_FALSE(LayersPaintingOverlayOverflowControlsAfter(child));
  EXPECT_EQ(child->GetLayoutObject().GetNode(), HitTest(99, 99));
}

TEST_P(ReorderOverlayOverflowControlsTest, AddRemoveStackedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #parent {
        position: relative;
        width: 100px;
        height: 100px;
      }
      #child {
        position: absolute;
        width: 200px;
        height: 200px;
        display: none;
      }
    </style>
    <div id='parent'>
      <div id='child'></div>
    </div>
  )HTML");

  InitOverflowStyle("parent");
  auto* parent = GetPaintLayerByElementId("parent");
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  auto* child_element = GetDocument().getElementById(AtomicString("child"));
  child_element->setAttribute(html_names::kStyleAttr,
                              AtomicString("display: block"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_THAT(LayersPaintingOverlayOverflowControlsAfter(
                  GetPaintLayerByElementId("child")),
              Pointee(ElementsAre(parent)));
  EXPECT_TRUE(parent->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->SelfNeedsRepaint());
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));

  child_element->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(parent->NeedsReorderOverlayOverflowControls());
  EXPECT_TRUE(parent->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(parent->SelfNeedsRepaint());
  EXPECT_EQ(parent->GetLayoutObject().GetNode(), HitTest(99, 99));
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

  EXPECT_TRUE(parent->SupportsSubsequenceCaching());
  EXPECT_TRUE(child1->SupportsSubsequenceCaching());
  EXPECT_TRUE(child2->SupportsSubsequenceCaching());
  EXPECT_TRUE(grandchild1->SupportsSubsequenceCaching());
  EXPECT_TRUE(grandchild2->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, SubsequenceCachingSVG) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svgroot'>
      <foreignObject id='foreignObject'/>
    </svg>
  )HTML");

  PaintLayer* svgroot = GetPaintLayerByElementId("svgroot");
  PaintLayer* foreign_object = GetPaintLayerByElementId("foreignObject");
  EXPECT_TRUE(svgroot->SupportsSubsequenceCaching());
  EXPECT_TRUE(foreign_object->SupportsSubsequenceCaching());
}

TEST_P(PaintLayerTest, SubsequenceCachingMuticol) {
  SetBodyInnerHTML(R"HTML(
    <div style='columns: 2'>
      <div id='target' style='position: relative; height: 20px;'></div>
    </div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_FALSE(target->SupportsSubsequenceCaching());
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
      PaintLayerPaintOrderIterator(target, kNegativeZOrderChildren).Next());
  EXPECT_FALSE(
      PaintLayerPaintOrderIterator(target, kPositiveZOrderChildren).Next());

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("z-index: 1"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(
      PaintLayerPaintOrderIterator(target, kNegativeZOrderChildren).Next());
  EXPECT_TRUE(
      PaintLayerPaintOrderIterator(target, kPositiveZOrderChildren).Next());
}

TEST_P(PaintLayerTest, HasVisibleSelfPaintingDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id='invisible' style='position:relative'>
      <div id='visible' style='visibility: visible; position: relative'>
      </div>
    </div>
  )HTML");
  PaintLayer* invisible = GetPaintLayerByElementId("invisible");
  PaintLayer* visible = GetPaintLayerByElementId("visible");

  EXPECT_TRUE(invisible->HasVisibleSelfPaintingDescendant());
  EXPECT_FALSE(visible->HasVisibleSelfPaintingDescendant());
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

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("transform: translateZ(1px)"));
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
  auto* iframe = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  iframe->setAttribute(html_names::kStyleAttr,
                       AtomicString("transform: translateY(5555px)"));
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
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
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
}

TEST_P(PaintLayerTest, CompositingContainerColumnSpanAll) {
  SetBodyInnerHTML(R"HTML(
    <div>
      <div id='multicol' style='columns: 1; position: relative'>
        <div id='paintContainer' style='position: relative'>
          <div id='columnSpan' style='column-span: all; overflow: hidden'></div>
        </div>
      </div>
    </div>
  )HTML");

  PaintLayer* columnSpan = GetPaintLayerByElementId("columnSpan");
  EXPECT_EQ(GetPaintLayerByElementId("paintContainer"),
            columnSpan->CompositingContainer());
  EXPECT_EQ(GetPaintLayerByElementId("multicol"),
            columnSpan->ContainingLayer());
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
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
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
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
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
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
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
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
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
  EXPECT_EQ(span, floating->ContainingLayer());
  EXPECT_EQ(span, absolute->Parent());
  EXPECT_EQ(span, absolute->ContainingLayer());
  EXPECT_EQ(container, span->Parent());
  EXPECT_EQ(container, span->ContainingLayer());
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
  EXPECT_EQ(span, floating->ContainingLayer());
  EXPECT_EQ(container, span->Parent());
  EXPECT_EQ(container, span->ContainingLayer());
  EXPECT_EQ(gfx::Vector2d(0, 400),
            span->ContainingLayer()->PixelSnappedScrolledContentOffset());
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
  EXPECT_EQ(span, floating->ContainingLayer());
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
  EXPECT_EQ(span, floating_parent->ContainingLayer());
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
  EXPECT_EQ(span, child->ContainingLayer());
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
  EXPECT_EQ(GetPaintLayerByElementId("span"), target->CompositingContainer());
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
  EXPECT_EQ(columns, extra_layer->Parent());
  EXPECT_EQ(columns, extra_layer->ContainingLayer());
  EXPECT_EQ(gfx::Vector2d(200, 0),
            spanner->ContainingLayer()->PixelSnappedScrolledContentOffset());
}

TEST_P(PaintLayerTest, PaintLayerTransformUpdatedOnStyleTransformAnimation) {
  SetBodyInnerHTML("<div id='target' style='will-change: transform'></div>");

  LayoutObject* target_object =
      GetDocument().getElementById(AtomicString("target"))->GetLayoutObject();
  PaintLayer* target_paint_layer =
      To<LayoutBoxModelObject>(target_object)->Layer();
  EXPECT_EQ(nullptr, target_paint_layer->Transform());

  const ComputedStyle* old_style = target_object->Style();
  ComputedStyleBuilder new_style_builder(*old_style);
  new_style_builder.SetHasCurrentTransformAnimation(true);
  target_object->SetStyle(new_style_builder.TakeStyle());

  EXPECT_NE(nullptr, target_paint_layer->Transform());
}

TEST_P(PaintLayerTest, NeedsRepaintOnSelfPaintingStatusChange) {
  SetBodyInnerHTML(R"HTML(
    <span id='span' style='opacity: 0.1'>
      <div id='target' style='overflow: hidden; float: left;
          position: relative;'>
      </div>
    </span>
  )HTML");

  auto* span_layer = GetPaintLayerByElementId("span");
  auto* target_element = GetDocument().getElementById(AtomicString("target"));
  auto* target_object = target_element->GetLayoutObject();
  auto* target_layer = To<LayoutBoxModelObject>(target_object)->Layer();

  // Target layer is self painting because it is relatively positioned.
  EXPECT_TRUE(target_layer->IsSelfPaintingLayer());
  EXPECT_EQ(span_layer, target_layer->CompositingContainer());
  EXPECT_FALSE(target_layer->SelfNeedsRepaint());
  EXPECT_FALSE(span_layer->SelfNeedsRepaint());

  // Removing position:relative makes target layer no longer self-painting,
  // and change its compositing container. The original compositing container
  // span_layer should be marked SelfNeedsRepaint.
  target_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("overflow: hidden; float: left"));

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(target_layer->IsSelfPaintingLayer());
  EXPECT_EQ(span_layer, target_layer->CompositingContainer());
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
  auto* target_element = GetDocument().getElementById(AtomicString("target"));
  auto* target_object = target_element->GetLayoutObject();
  auto* target_layer = To<LayoutBoxModelObject>(target_object)->Layer();

  // |container| is not the CompositingContainer of |target| because |target|
  // is stacked but |container| is not a stacking context.
  EXPECT_TRUE(target_layer->GetLayoutObject().IsStacked());
  EXPECT_NE(body_layer, target_layer->CompositingContainer());
  auto* old_compositing_container = target_layer->CompositingContainer();

  body->setAttribute(html_names::kStyleAttr, AtomicString("margin-top: 0"));
  target_element->setAttribute(html_names::kStyleAttr, AtomicString("top: 0"));
  UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_FALSE(target_object->HasLayer());
  EXPECT_TRUE(body_layer->SelfNeedsRepaint());
  EXPECT_TRUE(old_compositing_container->DescendantNeedsRepaint());

  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerTest, FrameViewContentSize) {
  SetBodyInnerHTML(
      "<style> body { width: 1200px; height: 900px; margin: 0 } </style>");
  EXPECT_EQ(gfx::Size(800, 600), GetDocument().View()->Size());
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

  auto* content = GetDocument().getElementById(AtomicString("content"));
  auto* body = GetDocument().body();

  // A hit test on the content div within the clip should hit it.
  EXPECT_EQ(content, GetDocument().ElementFromPoint(125, 75));
  EXPECT_EQ(content, GetDocument().ElementFromPoint(75, 125));

  // A hit test on the content div outside the clip should not hit it.
  EXPECT_EQ(body, GetDocument().ElementFromPoint(151, 60));
  EXPECT_EQ(body, GetDocument().ElementFromPoint(60, 151));

  // Zoom the page by 2x,
  GetDocument().GetFrame()->SetLayoutZoomFactor(2);

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

  auto* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(target, GetDocument().ElementFromPoint(280, 30));
}

TEST_P(PaintLayerTest, HitTestWithIgnoreClipping) {
  SetBodyInnerHTML("<div id='hit' style='width: 90px; height: 9000px;'></div>");

  HitTestRequest request(HitTestRequest::kIgnoreClipping);
  // (10, 900) is outside the viewport clip of 800x600.
  HitTestLocation location((gfx::Point(10, 900)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(GetDocument().getElementById(AtomicString("hit")),
            result.InnerNode());
}

TEST_P(PaintLayerTest, HitTestWithStopNode) {
  SetBodyInnerHTML(R"HTML(
    <div id='hit' style='width: 100px; height: 100px;'>
      <div id='child' style='width:100px;height:100px'></div>
    </div>
    <div id='overlap' style='position:relative;top:-50px;width:100px;height:100px'></div>
  )HTML");
  Element* hit = GetDocument().getElementById(AtomicString("hit"));
  Element* child = GetDocument().getElementById(AtomicString("child"));
  Element* overlap = GetDocument().getElementById(AtomicString("overlap"));

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
  Element* table = GetDocument().getElementById(AtomicString("table"));
  Element* cell11 = GetDocument().getElementById(AtomicString("cell11"));
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
  Element* svg = GetDocument().getElementById(AtomicString("svg"));
  Element* circle = GetDocument().getElementById(AtomicString("circle"));
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

  EXPECT_FALSE(html_layer->SelfNeedsRepaint());
  EXPECT_FALSE(span_layer->SelfNeedsRepaint());
  EXPECT_FALSE(floating_layer->SelfNeedsRepaint());
  EXPECT_FALSE(floating_layer->SelfNeedsRepaint());
  floating_layer->SetNeedsRepaint();
  EXPECT_TRUE(html_layer->DescendantNeedsRepaint());
  EXPECT_TRUE(span_layer->DescendantNeedsRepaint());
  EXPECT_TRUE(floating_layer->SelfNeedsRepaint());
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
  Element* target = GetDocument().getElementById(AtomicString("target"));
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
  Element* target = GetDocument().getElementById(AtomicString("target"));
  Element* container = GetDocument().getElementById(AtomicString("container"));
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
  Element* target = GetDocument().getElementById(AtomicString("target"));
  Element* container = GetDocument().getElementById(AtomicString("container"));
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
  Node* target =
      GetDocument().getElementById(AtomicString("target"))->firstChild();
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
  Element* target = GetDocument().getElementById(AtomicString("target"));
  Element* container = GetDocument().getElementById(AtomicString("container"));
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
    target_element->setAttribute(html_names::kStyleAttr,
                                 AtomicString("display: block"));
    UpdateAllLifecyclePhasesForTest();

    HitTestRequest request(HitTestRequest::kIgnoreClipping);
    HitTestLocation location((gfx::Point(198, 198)));
    HitTestResult result(request, location);
    GetDocument().GetLayoutView()->HitTest(location, result);
    if (i == 0)
      EXPECT_NE(target_element, result.InnerNode());
    else
      EXPECT_EQ(target_element, result.InnerNode());

    target_element->setAttribute(html_names::kStyleAttr,
                                 AtomicString("display: none"));
  }
}

TEST_P(PaintLayerTest, HitTestScrollbarUnderClip) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 50px; }</style>
    <div style="overflow: hidden; width: 200px; height: 100px">
      <div id="target" style="width: 200px; height: 200px; overflow: scroll">
        <!-- This relative div triggers crbug.com/1360860. -->
        <div style="position: relative"></div>
      </div>
    </div>
    <div id="below" style="height: 200px"></div>
  )HTML");

  // Hit the visible part of the vertical scrollbar.
  EXPECT_EQ(GetDocument().getElementById(AtomicString("target")),
            HitTest(245, 100));
  // Should not hit the hidden part of the vertical scrollbar, the hidden
  // horizontal scrollbar, or the hidden scroll corner.
  EXPECT_EQ(GetDocument().getElementById(AtomicString("below")),
            HitTest(245, 200));
  EXPECT_EQ(GetDocument().getElementById(AtomicString("below")),
            HitTest(150, 245));
  EXPECT_EQ(GetDocument().getElementById(AtomicString("below")),
            HitTest(245, 245));
}

TEST_P(PaintLayerTest, HitTestPerspectiveBackfaceHiddenNotInverted) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div style="transform: translate3d(50px, 80px, 200px);
                transform-style: preserve-3d; perspective: 100px;">
      <div id="target" style="width: 100px; height: 100px; background: green;
                              backface-visibility: hidden"></div>
    </div>
  )HTML");

  EXPECT_EQ(GetDocument().body(), HitTest(49, 79));
  EXPECT_EQ(GetDocument().getElementById(AtomicString("target")),
            HitTest(50, 80));
  EXPECT_EQ(GetDocument().getElementById(AtomicString("target")),
            HitTest(149, 179));
  EXPECT_EQ(GetDocument().documentElement(), HitTest(150, 180));
}

TEST_P(PaintLayerTest, HitTestObscuredOverlayScrollbar) {
  SetBodyInnerHTML(R"HTML(
    <div id="scroll" style="position: absolute; width: 200px; height: 200px;
                            top: 0; left: 0; overflow: scroll">
      <div style="position: relative; height: 400px"></div>
    </div>
    <div id="above" style="position: absolute; left: 100px; top: 100px;
                           width: 200px; height: 200px"></div>
  )HTML");

  auto* scroller = GetDocument().getElementById(AtomicString("scroll"));
  scroller->GetLayoutBox()->GetScrollableArea()->SetScrollbarsHiddenForTesting(
      false);
  EXPECT_EQ(scroller, HitTest(199, 1));
  EXPECT_EQ(GetDocument().getElementById(AtomicString("above")),
            HitTest(199, 101));
}

TEST_P(PaintLayerTest, InlineWithBackdropFilterHasPaintLayer) {
  SetBodyInnerHTML(
      "<map id='target' style='backdrop-filter: invert(1);'></map>");
  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  PaintLayer* root_layer = GetLayoutView().Layer();

  EXPECT_NE(nullptr, root_layer);
  EXPECT_NE(nullptr, paint_layer);
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
  const HitTestLocation location(gfx::Point(400, 300));
  HitTestResult result;
  GetLayoutView().HitTestNoLifecycleUpdate(location, result);
  EXPECT_EQ(result.InnerNode(), GetDocument().documentElement());
  EXPECT_EQ(result.GetScrollbar(), nullptr);

  if (GetDocument().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    const HitTestLocation location_scrollbar(gfx::Point(790, 300));
    HitTestResult result_scrollbar;
    EXPECT_EQ(result_scrollbar.InnerNode(), &GetDocument());
    EXPECT_NE(result_scrollbar.GetScrollbar(), nullptr);
  }
}

TEST_P(PaintLayerTest, HitTestTinyLayerUnderLargeScale) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 1px; height: 1px;
                            transform: scale(200); transform-origin: 0 0">
    </div>
  )HTML");

  auto* target = GetDocument().getElementById(AtomicString("target"));
  // Before https://crrev.com/c/4250297,
  // HitTestingTransformState::BoundsOfMappedQuadInternal() might "randomly"
  // return an empty rect with some of the following hit test locations.
  // See https://crbug.com/1414042.
  for (float x = 50; x < 50.5; x += 0.001) {
    const HitTestLocation location(gfx::PointF(x, 50));
    HitTestResult result;
    GetLayoutView().HitTest(location, result);
    EXPECT_EQ(target, result.InnerNode()) << " x=" << x;
  }
}

TEST_P(PaintLayerTest, AddLayerNeedsRepaintAndCullRectUpdate) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent" style="opacity: 0.9">
      <div id="child"></div>
  )HTML");

  auto* parent_layer = GetPaintLayerByElementId("parent");
  EXPECT_FALSE(parent_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(parent_layer->DescendantNeedsCullRectUpdate());
  auto* child = GetLayoutBoxByElementId("child");
  EXPECT_FALSE(child->HasLayer());

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("position: relative"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(parent_layer->DescendantNeedsRepaint());
  EXPECT_TRUE(parent_layer->DescendantNeedsCullRectUpdate());

  auto* child_layer = child->Layer();
  ASSERT_TRUE(child_layer);
  EXPECT_TRUE(child_layer->SelfNeedsRepaint());
  EXPECT_TRUE(child_layer->NeedsCullRectUpdate());
}

TEST_P(PaintLayerTest, HitTestLayerWith3DDescendantCrash) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="transform: translate(0)">
      <div style="transform-style: preserve-3d; transform: rotateY(1deg)"></div>
    </div>
  )HTML");

  auto* target = GetPaintLayerByElementId("target");
  EXPECT_TRUE(target->Has3DTransformedDescendant());
  HitTestRequest request(0);
  HitTestLocation location;
  HitTestResult result(request, location);
  // This should not crash.
  target->HitTest(location, result, PhysicalRect(0, 0, 800, 600));
}

#define TEST_SCROLL_CONTAINER(name, expected_scroll_container,           \
                              expected_is_fixed_to_view)                 \
  do {                                                                   \
    auto* layer = GetPaintLayerByElementId(name);                        \
    bool is_fixed_to_view = false;                                       \
    EXPECT_EQ(expected_scroll_container,                                 \
              layer->ContainingScrollContainerLayer(&is_fixed_to_view)); \
    EXPECT_EQ(expected_is_fixed_to_view, is_fixed_to_view);              \
  } while (false)

TEST_P(PaintLayerTest, ScrollContainerLayerRootScroller) {
  SetBodyInnerHTML(R"HTML(
    <div id="sticky" style="position: sticky"></div>
    <div id="absolute" style="position: absolute"></div>
    <div id="fixed" style="position: fixed">
      <div id="sticky-under-fixed" style="position: sticky"></div>
      <div id="absolute-under-fixed" style="position: absolute"></div>
      <div id="fixed-under-fixed" style="position: fixed">
        <div id="sticky-under-nested-fixed" style="position: sticky"></div>
        <div id="absolute-under-nested-fixed" style="position: absolute"></div>
        <div id="fixed-under-nested-fixed" style="position: fixed"></div>
        <div id="transform-under-nested-fixed" style="transform: rotate(1deg)">
        </div>
      </div>
      <div id="transform-under-fixed" style="transform: rotate(1deg)"></div>
    </div>
    <div id="transform" style="transform: rotate(1deg)">
      <div id="sticky-under-transform" style="position: sticky"></div>
      <div id="absolute-under-transform" style="position: absolute"></div>
      <div id="fixed-under-transform" style="position: fixed"></div>
      <div id="transform-under-transform" style="transform: rotate(1deg)"></div>
    </div>
  )HTML");

  auto* view_layer = GetLayoutView().Layer();
  {
    bool is_fixed_to_view = false;
    EXPECT_EQ(nullptr,
              view_layer->ContainingScrollContainerLayer(&is_fixed_to_view));
    EXPECT_TRUE(is_fixed_to_view);
  }

  TEST_SCROLL_CONTAINER("sticky", view_layer, false);
  TEST_SCROLL_CONTAINER("absolute", view_layer, false);
  TEST_SCROLL_CONTAINER("fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("transform", view_layer, false);

  TEST_SCROLL_CONTAINER("sticky-under-fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("absolute-under-fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("fixed-under-fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("transform-under-fixed", view_layer, true);

  TEST_SCROLL_CONTAINER("sticky-under-nested-fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("absolute-under-nested-fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("fixed-under-nested-fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("transform-under-nested-fixed", view_layer, true);

  TEST_SCROLL_CONTAINER("sticky-under-transform", view_layer, false);
  TEST_SCROLL_CONTAINER("absolute-under-transform", view_layer, false);
  TEST_SCROLL_CONTAINER("fixed-under-transform", view_layer, false);
  TEST_SCROLL_CONTAINER("transform-under-transform", view_layer, false);
}

TEST_P(PaintLayerTest, ScrollContainerLayerRelativeScroller) {
  SetBodyInnerHTML(R"HTML(
    <div id="scroller" style="width: 100px; height: 100px; overflow: scroll;
                              position: relative">
      <div id="sticky" style="position: sticky">
        <div id="sticky-under-sticky" style="position: sticky"></div>
        <div id="absolute-under-sticky" style="position: absolute"></div>
        <div id="fixed-under-sticky" style="position: fixed"></div>
        <div id="transform-under-sticky" style="transform: rotate(1deg)"></div>
      </div>
      <div id="absolute" style="position: absolute">
        <div id="sticky-under-absolute" style="position: sticky"></div>
        <div id="absolute-under-absolute" style="position: absolute"></div>
        <div id="fixed-under-absolute" style="position: fixed"></div>
        <div id="transform-under-absolute" style="transform: rotate(1deg)">
        </div>
      </div>
      <div id="fixed" style="position: fixed">
        <div id="sticky-under-fixed" style="position: sticky"></div>
        <div id="absolute-under-fixed" style="position: absolute"></div>
        <div id="fixed-under-fixed" style="position: fixed"></div>
        <div id="transform-under-fixed" style="transform: rotate(1deg)"></div>
      </div>
      <div id="transform" style="transform: rotate(1deg)">
        <div id="sticky-under-transform" style="position: sticky"></div>
        <div id="absolute-under-transform" style="position: absolute"></div>
        <div id="fixed-under-transform" style="position: fixed"></div>
        <div id="transform-under-transform" style="transform: rotate(1deg)">
        </div>
      </div>
  )HTML");

  auto* view_layer = GetLayoutView().Layer();
  // scroller has relative position so contains absolute but not fixed.
  auto* scroller = GetPaintLayerByElementId("scroller");
  ASSERT_TRUE(scroller->GetLayoutObject().CanContainAbsolutePositionObjects());
  ASSERT_FALSE(scroller->GetLayoutObject().CanContainFixedPositionObjects());
  TEST_SCROLL_CONTAINER("scroller", view_layer, false);

  TEST_SCROLL_CONTAINER("sticky", scroller, false);
  TEST_SCROLL_CONTAINER("sticky-under-sticky", scroller, false);
  TEST_SCROLL_CONTAINER("absolute-under-sticky", scroller, false);
  TEST_SCROLL_CONTAINER("fixed-under-sticky", view_layer, true);
  TEST_SCROLL_CONTAINER("transform-under-sticky", scroller, false);

  TEST_SCROLL_CONTAINER("absolute", scroller, false);
  TEST_SCROLL_CONTAINER("sticky-under-absolute", scroller, false);
  TEST_SCROLL_CONTAINER("absolute-under-absolute", scroller, false);
  TEST_SCROLL_CONTAINER("fixed-under-absolute", view_layer, true);
  TEST_SCROLL_CONTAINER("transform-under-absolute", scroller, false);

  TEST_SCROLL_CONTAINER("fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("sticky-under-fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("absolute-under-fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("fixed-under-fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("transform-under-fixed", view_layer, true);

  TEST_SCROLL_CONTAINER("transform", scroller, false);
  TEST_SCROLL_CONTAINER("sticky-under-transform", scroller, false);
  TEST_SCROLL_CONTAINER("absolute-under-transform", scroller, false);
  TEST_SCROLL_CONTAINER("fixed-under-transform", scroller, false);
  TEST_SCROLL_CONTAINER("transform-under-transform", scroller, false);
}

TEST_P(PaintLayerTest, ScrollContainerLayerNestedScroller) {
  SetBodyInnerHTML(R"HTML(
    <div id="scroller1" style="width: 100px; height: 100px; overflow: scroll;
                               position: relative">
      <div id="scroller2" style="width: 100px; height: 100px; overflow: scroll">
        <div id="sticky" style="position: sticky"></div>
        <div id="absolute" style="position: absolute"></div>
        <div id="fixed" style="position: fixed"></div>
        <div id="transform" style="transform: rotate(1deg"></div>
      </div>
    </div>
  )HTML");

  auto* view_layer = GetLayoutView().Layer();
  // scroller1 has relative position so contains absolute but not fixed.
  // scroller2 is static position so contains neither absolute or fixed.
  auto* scroller1 = GetPaintLayerByElementId("scroller1");
  auto* scroller2 = GetPaintLayerByElementId("scroller2");
  ASSERT_FALSE(
      scroller2->GetLayoutObject().CanContainAbsolutePositionObjects());
  ASSERT_FALSE(scroller2->GetLayoutObject().CanContainFixedPositionObjects());
  TEST_SCROLL_CONTAINER("scroller2", scroller1, false);

  TEST_SCROLL_CONTAINER("sticky", scroller2, false);
  TEST_SCROLL_CONTAINER("absolute", scroller1, false);
  TEST_SCROLL_CONTAINER("fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("transform", scroller2, false);
}

TEST_P(PaintLayerTest, ScrollContainerLayerScrollerUnderRealFixed) {
  SetBodyInnerHTML(R"HTML(
    <div style="position: fixed">
      <div id="scroller" style="width: 100px; height: 100px; overflow: scroll">
        <div id="sticky" style="position: sticky"></div>
        <div id="absolute" style="position: absolute"></div>
        <div id="fixed" style="position: fixed"></div>
        <div id="transform" style="transform: rotate(1deg"></div>
      </div>
    </div>
  )HTML");

  auto* view_layer = GetLayoutView().Layer();
  // scroller is static_position, under real position:fixed.
  auto* scroller = GetPaintLayerByElementId("scroller");
  TEST_SCROLL_CONTAINER("scroller", view_layer, true);
  TEST_SCROLL_CONTAINER("sticky", scroller, false);
  TEST_SCROLL_CONTAINER("absolute", view_layer, true);
  TEST_SCROLL_CONTAINER("fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("transform", scroller, false);
}

TEST_P(PaintLayerTest, ScrollContainerLayerScrollerUnderFakeFixed) {
  SetBodyInnerHTML(R"HTML(
    <div style="transform: rotate(1deg)">
      <div style="position: fixed">
        <div id="scroller"
             style="width: 100px; height: 100px; overflow: scroll">
          <div id="sticky" style="position: sticky"></div>
          <div id="absolute" style="position: absolute"></div>
          <div id="fixed" style="position: fixed"></div>
          <div id="transform" style="transform: rotate(1deg"></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* view_layer = GetLayoutView().Layer();
  // scroller is static position, under fake position:fixed.
  auto* scroller = GetPaintLayerByElementId("scroller");
  TEST_SCROLL_CONTAINER("scroller", view_layer, false);
  TEST_SCROLL_CONTAINER("sticky", scroller, false);
  TEST_SCROLL_CONTAINER("absolute", view_layer, false);
  TEST_SCROLL_CONTAINER("fixed", view_layer, false);
  TEST_SCROLL_CONTAINER("transform", scroller, false);
}

TEST_P(PaintLayerTest, ScrollContainerLayerFixedScroller) {
  SetBodyInnerHTML(R"HTML(
    <div id="scroller"
         style="position: fixed; width: 100px; height: 100px; overflow: scroll">
      <div id="sticky" style="position: sticky"></div>
      <div id="absolute" style="position: absolute"></div>
      <div id="fixed" style="position: fixed"></div>
      <div id="transform" style="transform: rotate(1deg"></div>
    </div>
  )HTML");

  auto* view_layer = GetLayoutView().Layer();
  // scroller itself has real fixed position.
  auto* scroller = GetPaintLayerByElementId("scroller");
  TEST_SCROLL_CONTAINER("scroller", view_layer, true);
  TEST_SCROLL_CONTAINER("sticky", scroller, false);
  TEST_SCROLL_CONTAINER("absolute", scroller, false);
  TEST_SCROLL_CONTAINER("fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("transform", scroller, false);
}

TEST_P(PaintLayerTest, ScrollContainerLayerScrollerUnderTransformAndFixed) {
  SetBodyInnerHTML(R"HTML(
    <div style="transform: rotate(1deg); position: fixed">
      <div id="scroller" style="width: 100px; height: 100px; overflow: scroll">
        <div id="sticky" style="position: sticky"></div>
        <div id="absolute" style="position: absolute"></div>
        <div id="fixed" style="position: fixed"></div>
        <div id="transform" style="transform: rotate(1deg"></div>
      </div>
    </div>
  )HTML");

  auto* view_layer = GetLayoutView().Layer();
  auto* scroller = GetPaintLayerByElementId("scroller");
  TEST_SCROLL_CONTAINER("scroller", view_layer, true);
  TEST_SCROLL_CONTAINER("sticky", scroller, false);
  TEST_SCROLL_CONTAINER("absolute", view_layer, true);
  TEST_SCROLL_CONTAINER("fixed", view_layer, true);
  TEST_SCROLL_CONTAINER("transform", scroller, false);
}

TEST_P(PaintLayerTest, ScrollContainerLayerTransformScroller) {
  SetBodyInnerHTML(R"HTML(
    <div id="scroller" style="transform: rotate(1deg);
                              width: 100px; height: 100px; overflow: scroll">
      <div id="sticky" style="position: sticky"></div>
      <div id="absolute" style="position: absolute"></div>
      <div id="fixed" style="position: fixed"></div>
      <div id="transform" style="transform: rotate(1deg"></div>
    </div>
  )HTML");

  auto* view_layer = GetLayoutView().Layer();
  auto* scroller = GetPaintLayerByElementId("scroller");
  TEST_SCROLL_CONTAINER("scroller", view_layer, false);
  TEST_SCROLL_CONTAINER("sticky", scroller, false);
  TEST_SCROLL_CONTAINER("absolute", scroller, false);
  TEST_SCROLL_CONTAINER("fixed", scroller, false);
  TEST_SCROLL_CONTAINER("transform", scroller, false);
}

TEST_P(PaintLayerTest, HitTestScrollMarkerPseudoElement) {
  GetDocument().body()->setInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; scroll-marker-group: before; width: "
      "100px; height: 100px; }"
      "#scroller::scroll-marker-group { border: 3px solid black; display: "
      "flex; width: 100px; height: 20px; }"
      "#scroller div { width: 100px; height: 100px; background: green; }"
      "#scroller div::scroll-marker { content: ''; display: inline-flex; "
      "width: 10px; height: 10px; background: green; border-radius: 50%; }"
      "</style>"
      "<div id='scroller'>"
      "  <div></div>"
      "  <div id='second_div'></div>"
      "</div>");
  UpdateAllLifecyclePhasesForTest();
  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  EXPECT_EQ(scroller->scrollTop(), 0);
  Element* second_div =
      GetDocument().getElementById(AtomicString("second_div"));
  PseudoElement* second_scroll_marker =
      second_div->GetPseudoElement(kPseudoIdScrollMarker);

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(PhysicalOffset(25, 20));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(second_scroll_marker, result.InnerNode());

  MouseEvent& event = *MouseEvent::Create();
  event.SetType(event_type_names::kClick);
  event.SetTarget(second_scroll_marker);
  second_scroll_marker->DefaultEventHandler(event);
  EXPECT_EQ(scroller->scrollTop(), 100);
}

}  // namespace blink
