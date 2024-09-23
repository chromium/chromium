// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"

#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class PrePaintTreeWalkTest : public PaintControllerPaintTest {
 public:
  const TransformPaintPropertyNode* FramePreTranslation() {
    return GetDocument()
        .View()
        ->GetLayoutView()
        ->FirstFragment()
        .PaintProperties()
        ->PaintOffsetTranslation();
  }

  const TransformPaintPropertyNode* FrameScrollTranslation() {
    return GetDocument()
        .View()
        ->GetLayoutView()
        ->FirstFragment()
        .PaintProperties()
        ->ScrollTranslation();
  }

 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(PrePaintTreeWalkTest);

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithBorderInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #transformed { transform: translate(100px, 100px); }
      .border { border: 10px solid black; }
    </style>
    <div id='transformed'></div>
  )HTML");

  auto* transformed_element =
      GetDocument().getElementById(AtomicString("transformed"));
  const auto* transformed_properties =
      transformed_element->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(100, 100),
            transformed_properties->Transform()->Get2dTranslation());

  // Artifically change the transform node.
  const_cast<ObjectPaintProperties*>(transformed_properties)->ClearTransform();
  EXPECT_EQ(nullptr, transformed_properties->Transform());

  // Cause a paint invalidation.
  transformed_element->setAttribute(html_names::kClassAttr,
                                    AtomicString("border"));
  UpdateAllLifecyclePhasesForTest();

  // Should have changed back.
  EXPECT_EQ(gfx::Vector2dF(100, 100),
            transformed_properties->Transform()->Get2dTranslation());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithFrameScroll) {
  SetBodyInnerHTML("<style> body { height: 10000px; } </style>");
  EXPECT_TRUE(FrameScrollTranslation()->IsIdentity());

  // Cause a scroll invalidation and ensure the translation is updated.
  GetDocument().domWindow()->scrollTo(0, 100);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(gfx::Vector2dF(0, -100),
            FrameScrollTranslation()->Get2dTranslation());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithCSSTransformInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .transformA { transform: translate(100px, 100px); }
      .transformB { transform: translate(200px, 200px); }
      #transformed { will-change: transform; }
    </style>
    <div id='transformed' class='transformA'></div>
  )HTML");

  auto* transformed_element =
      GetDocument().getElementById(AtomicString("transformed"));
  const auto* transformed_properties =
      transformed_element->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(100, 100),
            transformed_properties->Transform()->Get2dTranslation());

  // Invalidate the CSS transform property.
  transformed_element->setAttribute(html_names::kClassAttr,
                                    AtomicString("transformB"));
  UpdateAllLifecyclePhasesForTest();

  // The transform should have changed.
  EXPECT_EQ(gfx::Vector2dF(200, 200),
            transformed_properties->Transform()->Get2dTranslation());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithOpacityInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .opacityA { opacity: 0.9; }
      .opacityB { opacity: 0.4; }
    </style>
    <div id='transparent' class='opacityA'></div>
  )HTML");

  auto* transparent_element =
      GetDocument().getElementById(AtomicString("transparent"));
  const auto* transparent_properties =
      transparent_element->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(0.9f, transparent_properties->Effect()->Opacity());

  // Invalidate the opacity property.
  transparent_element->setAttribute(html_names::kClassAttr,
                                    AtomicString("opacityB"));
  UpdateAllLifecyclePhasesForTest();

  // The opacity should have changed.
  EXPECT_EQ(0.4f, transparent_properties->Effect()->Opacity());
}

TEST_P(PrePaintTreeWalkTest, ClearSubsequenceCachingClipChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .clip { overflow: hidden }
    </style>
    <div id='parent' style='transform: translateZ(0); width: 100px;
      height: 100px;'>
      <div id='child' style='isolation: isolate'>
        content
      </div>
    </div>
  )HTML");

  auto* parent = GetDocument().getElementById(AtomicString("parent"));
  auto* child_paint_layer = GetPaintLayerByElementId("child");
  EXPECT_FALSE(child_paint_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  parent->setAttribute(html_names::kClassAttr, AtomicString("clip"));
  UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->SelfNeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, ClearSubsequenceCachingClipChange2DTransform) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .clip { overflow: hidden }
    </style>
    <div id='parent' style='transform: translateX(0); width: 100px;
      height: 100px;'>
      <div id='child' style='isolation: isolate'>
        content
      </div>
    </div>
  )HTML");

  auto* parent = GetDocument().getElementById(AtomicString("parent"));
  auto* child_paint_layer = GetPaintLayerByElementId("child");
  EXPECT_FALSE(child_paint_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  parent->setAttribute(html_names::kClassAttr, AtomicString("clip"));
  UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->SelfNeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, ClearSubsequenceCachingClipChangePosAbs) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .clip { overflow: hidden }
    </style>
    <div id='parent' style='transform: translateZ(0); width: 100px;
      height: 100px; position: absolute'>
      <div id='child' style='overflow: hidden; position: relative;
          z-index: 0; width: 50px; height: 50px'>
        content
      </div>
    </div>
  )HTML");

  auto* parent = GetDocument().getElementById(AtomicString("parent"));
  auto* child_paint_layer = GetPaintLayerByElementId("child");
  EXPECT_FALSE(child_paint_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  // This changes clips for absolute-positioned descendants of "child" but not
  // normal-position ones, which are already clipped to 50x50.
  parent->setAttribute(html_names::kClassAttr, AtomicString("clip"));
  UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->SelfNeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, ClearSubsequenceCachingClipChangePosFixed) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .clip { overflow: hidden }
    </style>
    <div id='parent' style='transform: translateZ(0); width: 100px;
      height: 100px;'>
      <div id='child' style='overflow: hidden; z-index: 0;
          position: absolute; width: 50px; height: 50px'>
        content
      </div>
    </div>
  )HTML");

  auto* parent = GetDocument().getElementById(AtomicString("parent"));
  auto* child_paint_layer = GetPaintLayerByElementId("child");
  EXPECT_FALSE(child_paint_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  // This changes clips for absolute-positioned descendants of "child" but not
  // normal-position ones, which are already clipped to 50x50.
  parent->setAttribute(html_names::kClassAttr, AtomicString("clip"));
  UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->SelfNeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, ClipChangeRepaintsDescendants) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent { position: relative; width: 100px; }
      #child { overflow: hidden; width: 10%; height: 100%; position: relative; }
      #greatgrandchild {
        width: 100px; height: 100px; z-index: 100; position: relative;
      }
    </style>
    <div id='parent' style='height: 10px'>
      <div id='child'>
        <div id='grandchild'>
          <div id='greatgrandchild'></div>
        </div>
      </div>
    </div>
  )HTML");

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("height: 100px"));
  UpdateAllLifecyclePhasesExceptPaint();

  auto* paint_layer = GetPaintLayerByElementId("greatgrandchild");
  EXPECT_TRUE(paint_layer->SelfNeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, ClipChangeHasRadius) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        position: absolute;
        z-index: 0;
        overflow: hidden;
        width: 50px;
        height: 50px;
      }
    </style>
    <div id='target'></div>
  )HTML");

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* target_object = To<LayoutBoxModelObject>(target->GetLayoutObject());
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("border-radius: 5px"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(target_object->Layer()->SelfNeedsRepaint());
  // And should not trigger any assert failure.
  UpdateAllLifecyclePhasesForTest();
}

namespace {
class PrePaintTreeWalkMockEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) final {}
};
}  // namespace

TEST_P(PrePaintTreeWalkTest, InsideBlockingTouchEventHandlerUpdate) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor' style='width: 100px; height: 100px;'>
      <div id='handler' style='width: 100px; height: 100px;'>
        <div id='descendant' style='width: 100px; height: 100px;'>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  auto& ancestor = *GetLayoutObjectByElementId("ancestor");
  auto& handler = *GetLayoutObjectByElementId("handler");
  auto& descendant = *GetLayoutObjectByElementId("descendant");

  EXPECT_FALSE(ancestor.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor.InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler.InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant.InsideBlockingTouchEventHandler());

  PrePaintTreeWalkMockEventListener* callback =
      MakeGarbageCollected<PrePaintTreeWalkMockEventListener>();
  auto* handler_element = GetDocument().getElementById(AtomicString("handler"));
  handler_element->addEventListener(event_type_names::kTouchstart, callback);

  EXPECT_FALSE(ancestor.EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveAllowedTouchActionChanged());

  EXPECT_TRUE(ancestor.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveAllowedTouchActionChanged());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor.InsideBlockingTouchEventHandler());
  EXPECT_TRUE(handler.InsideBlockingTouchEventHandler());
  EXPECT_TRUE(descendant.InsideBlockingTouchEventHandler());
}

TEST_P(PrePaintTreeWalkTest, EffectiveTouchActionStyleUpdate) {
  SetBodyInnerHTML(R"HTML(
    <style> .touchaction { touch-action: none; } </style>
    <div id='ancestor' style='width: 100px; height: 100px;'>
      <div id='touchaction' style='width: 100px; height: 100px;'>
        <div id='descendant' style='width: 100px; height: 100px;'>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  auto& ancestor = *GetLayoutObjectByElementId("ancestor");
  auto& touchaction = *GetLayoutObjectByElementId("touchaction");
  auto& descendant = *GetLayoutObjectByElementId("descendant");

  EXPECT_FALSE(ancestor.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(touchaction.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(ancestor.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(touchaction.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveAllowedTouchActionChanged());

  GetDocument()
      .getElementById(AtomicString("touchaction"))
      ->setAttribute(html_names::kClassAttr, AtomicString("touchaction"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(ancestor.EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(touchaction.EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(descendant.EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(ancestor.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(touchaction.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveAllowedTouchActionChanged());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(touchaction.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(ancestor.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(touchaction.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveAllowedTouchActionChanged());
}

TEST_P(PrePaintTreeWalkTest, InsideBlockingWheelEventHandlerUpdate) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor' style='width: 100px; height: 100px;'>
      <div id='handler' style='width: 100px; height: 100px;'>
        <div id='descendant' style='width: 100px; height: 100px;'>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  auto& ancestor = *GetLayoutObjectByElementId("ancestor");
  auto& handler = *GetLayoutObjectByElementId("handler");
  auto& descendant = *GetLayoutObjectByElementId("descendant");

  EXPECT_FALSE(ancestor.BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler.BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant.BlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor.DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler.DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant.DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor.InsideBlockingWheelEventHandler());
  EXPECT_FALSE(handler.InsideBlockingWheelEventHandler());
  EXPECT_FALSE(descendant.InsideBlockingWheelEventHandler());

  PrePaintTreeWalkMockEventListener* callback =
      MakeGarbageCollected<PrePaintTreeWalkMockEventListener>();
  auto* handler_element = GetDocument().getElementById(AtomicString("handler"));
  handler_element->addEventListener(event_type_names::kWheel, callback);

  EXPECT_FALSE(ancestor.BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(handler.BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant.BlockingWheelEventHandlerChanged());

  EXPECT_TRUE(ancestor.DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler.DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant.DescendantBlockingWheelEventHandlerChanged());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor.BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler.BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant.BlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor.DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler.DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant.DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor.InsideBlockingWheelEventHandler());
  EXPECT_TRUE(handler.InsideBlockingWheelEventHandler());
  EXPECT_TRUE(descendant.InsideBlockingWheelEventHandler());
}

TEST_P(PrePaintTreeWalkTest, CullRectUpdateOnSVGTransformChange) {
  SetBodyInnerHTML(R"HTML(
    <svg style="width: 200px; height: 200px">
      <rect id="rect"/>
      <g id="g"><foreignObject id="foreign"/></g>
    </svg>
  )HTML");

  auto& foreign = *GetLayoutObjectByElementId("foreign");
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200),
            foreign.FirstFragment().GetCullRect().Rect());

  GetDocument()
      .getElementById(AtomicString("rect"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("transform: translateX(20px)"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200),
            foreign.FirstFragment().GetCullRect().Rect());

  GetDocument()
      .getElementById(AtomicString("g"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("transform: translateY(20px)"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(gfx::Rect(0, -20, 200, 200),
            foreign.FirstFragment().GetCullRect().Rect());
}

TEST_P(PrePaintTreeWalkTest, InlineOutlineWithContinuationPaintInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <div>
      <span style="outline: 1px solid black">
        <span id="child-span">span</span>
        <div>continuation</div>
      </span>
    </div>
  )HTML");

  // This test passes if the following doesn't crash.
  GetDocument()
      .getElementById(AtomicString("child-span"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("color: blue"));
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PrePaintTreeWalkTest, ScrollTranslationNodeForNonZeroScrollPosition) {
  SetBodyInnerHTML(R"HTML(
    <div id="div" style="overflow:hidden;max-width:5ch;direction:rtl">
      loremipsumdolorsitamet
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* scroller = GetDocument().getElementById(AtomicString("div"));
  auto* object = To<LayoutBoxModelObject>(scroller->GetLayoutObject());
  auto* scrollable_area = object->GetScrollableArea();

  ASSERT_EQ(ScrollOffset(), scrollable_area->GetScrollOffset());
  ASSERT_NE(gfx::PointF(), scrollable_area->ScrollPosition());
  EXPECT_TRUE(object->FirstFragment().PaintProperties()->ScrollTranslation());

  // When the scroll is scrolled all the way to the end of content it should
  // still get a scroll node.
  scroller->scrollBy(-10000, 0);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_NE(ScrollOffset(), scrollable_area->GetScrollOffset());
  ASSERT_EQ(gfx::PointF(), scrollable_area->ScrollPosition());
  EXPECT_TRUE(object->FirstFragment().PaintProperties()->ScrollTranslation());
}

}  // namespace blink
