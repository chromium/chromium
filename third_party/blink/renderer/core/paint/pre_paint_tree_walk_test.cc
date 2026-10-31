// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"

#include "cc/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing_paint_attribution_tracker.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics_test_util.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_paint_attribution_tracker.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
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

  auto* transformed_element = GetElementById("transformed");
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
  GetDocument().domWindow()->scrollToForTesting(0, 100);
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

  auto* transformed_element = GetElementById("transformed");
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

  auto* transparent_element = GetElementById("transparent");
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

  auto* parent = GetElementById("parent");
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

  auto* parent = GetElementById("parent");
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

  auto* parent = GetElementById("parent");
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

  auto* parent = GetElementById("parent");
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

  GetElementById("parent")->setAttribute(html_names::kStyleAttr,
                                         AtomicString("height: 100px"));
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

  auto* target = GetElementById("target");
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

  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(handler));
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(descendant));

  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(handler));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(descendant));

  EXPECT_FALSE(ancestor.InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler.InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant.InsideBlockingTouchEventHandler());

  PrePaintTreeWalkMockEventListener* callback =
      MakeGarbageCollected<PrePaintTreeWalkMockEventListener>();
  auto* handler_element = GetElementById("handler");
  handler_element->addEventListener(event_type_names::kTouchstart, callback);

  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_TRUE(EffectiveAllowedTouchActionChanged(handler));
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(descendant));

  EXPECT_TRUE(DescendantEffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(handler));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(descendant));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(handler));
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(descendant));

  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(handler));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(descendant));

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

  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(touchaction));
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(descendant));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(touchaction));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(descendant));

  GetElementById("touchaction")
      ->setAttribute(html_names::kClassAttr, AtomicString("touchaction"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_TRUE(EffectiveAllowedTouchActionChanged(touchaction));
  EXPECT_TRUE(EffectiveAllowedTouchActionChanged(descendant));
  EXPECT_TRUE(DescendantEffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_TRUE(DescendantEffectiveAllowedTouchActionChanged(touchaction));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(descendant));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(touchaction));
  EXPECT_FALSE(EffectiveAllowedTouchActionChanged(descendant));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(ancestor));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(touchaction));
  EXPECT_FALSE(DescendantEffectiveAllowedTouchActionChanged(descendant));
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

  EXPECT_FALSE(BlockingWheelEventHandlerChanged(ancestor));
  EXPECT_FALSE(BlockingWheelEventHandlerChanged(handler));
  EXPECT_FALSE(BlockingWheelEventHandlerChanged(descendant));

  EXPECT_FALSE(DescendantBlockingWheelEventHandlerChanged(ancestor));
  EXPECT_FALSE(DescendantBlockingWheelEventHandlerChanged(handler));
  EXPECT_FALSE(DescendantBlockingWheelEventHandlerChanged(descendant));

  EXPECT_FALSE(ancestor.InsideBlockingWheelEventHandler());
  EXPECT_FALSE(handler.InsideBlockingWheelEventHandler());
  EXPECT_FALSE(descendant.InsideBlockingWheelEventHandler());

  PrePaintTreeWalkMockEventListener* callback =
      MakeGarbageCollected<PrePaintTreeWalkMockEventListener>();
  auto* handler_element = GetElementById("handler");
  handler_element->addEventListener(event_type_names::kWheel, callback);

  EXPECT_FALSE(BlockingWheelEventHandlerChanged(ancestor));
  EXPECT_TRUE(BlockingWheelEventHandlerChanged(handler));
  EXPECT_FALSE(BlockingWheelEventHandlerChanged(descendant));

  EXPECT_TRUE(DescendantBlockingWheelEventHandlerChanged(ancestor));
  EXPECT_FALSE(DescendantBlockingWheelEventHandlerChanged(handler));
  EXPECT_FALSE(DescendantBlockingWheelEventHandlerChanged(descendant));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(BlockingWheelEventHandlerChanged(ancestor));
  EXPECT_FALSE(BlockingWheelEventHandlerChanged(handler));
  EXPECT_FALSE(BlockingWheelEventHandlerChanged(descendant));

  EXPECT_FALSE(DescendantBlockingWheelEventHandlerChanged(ancestor));
  EXPECT_FALSE(DescendantBlockingWheelEventHandlerChanged(handler));
  EXPECT_FALSE(DescendantBlockingWheelEventHandlerChanged(descendant));

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

  GetElementById("rect")->setAttribute(
      html_names::kStyleAttr, AtomicString("transform: translateX(20px)"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200),
            foreign.FirstFragment().GetCullRect().Rect());

  GetElementById("g")->setAttribute(
      html_names::kStyleAttr, AtomicString("transform: translateY(20px)"));
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
  GetElementById("child-span")
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

  auto* scroller = GetElementById("div");
  auto* object = To<LayoutBoxModelObject>(scroller->GetLayoutObject());
  auto* scrollable_area = object->GetScrollableArea();

  ASSERT_EQ(ScrollOffset(), scrollable_area->GetScrollOffset());
  ASSERT_NE(gfx::PointF(), scrollable_area->ScrollPosition());
  EXPECT_TRUE(object->FirstFragment().PaintProperties()->ScrollTranslation());

  // When the scroll is scrolled all the way to the end of content it should
  // still get a scroll node.
  scroller->scrollByForTesting(-10000, 0);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_NE(ScrollOffset(), scrollable_area->GetScrollOffset());
  ASSERT_EQ(gfx::PointF(), scrollable_area->ScrollPosition());
  EXPECT_TRUE(object->FirstFragment().PaintProperties()->ScrollTranslation());
}

TEST_P(PrePaintTreeWalkTest,
       ForcedSubtreeUpdateReasonsPersistedAcrossDisplayLock) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
      }
      #wrapper {
        contain: paint;
        border-radius: 1px;
      }
      #lock {
        contain: paint;
      }
      #target {
        opacity: 0.5;
      }
    </style>
    <canvas content="drawable" width="100" height="100">
      <div id="wrapper">
        <div id="lock">
          <div id="target"></div>
        </div>
      </div>
    </canvas>
  )HTML");

  auto* wrapper = GetDocument().getElementById(AtomicString("wrapper"));
  auto* lock = GetDocument().getElementById(AtomicString("lock"));
  const auto* target_properties =
      GetLayoutObjectByElementId("target")->FirstFragment().PaintProperties();
  ASSERT_TRUE(target_properties);
  ASSERT_TRUE(target_properties->Effect());
  EXPECT_FALSE(target_properties->Effect()->IsInDrawableCanvasSubtree());

  // Lock #lock and make #wrapper drawable in the same lifecycle update. The
  // drawable change forces an isolation-piercing subtree paint property update,
  // which is blocked at #lock.
  lock->setAttribute(html_names::kStyleAttr,
                     AtomicString("content-visibility: hidden"));
  wrapper->setAttribute(html_names::kDrawableAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(
      GetLayoutObjectByElementId("lock")->SubtreePaintPropertyUpdateReasons(),
      static_cast<unsigned>(
          SubtreePaintPropertyUpdateReason::kPreviouslySkipped));
  EXPECT_FALSE(target_properties->Effect()->IsInDrawableCanvasSubtree());

  // Dirty #wrapper before unlocking #lock so that unlocking #lock does not
  // propagate NeedsPaintPropertyUpdate() up to the <canvas>, then unlock #lock.
  wrapper->setAttribute(html_names::kStyleAttr,
                        AtomicString("border-radius: 2px"));
  lock->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(
      GetLayoutObjectByElementId("lock")->SubtreePaintPropertyUpdateReasons(),
      static_cast<unsigned>(SubtreePaintPropertyUpdateReason::kNone));
  EXPECT_TRUE(target_properties->Effect()->IsInDrawableCanvasSubtree());
}

class SoftNavigationPrePaintTreeWalkTest : public RenderingTest {
 public:
  SoftNavigationPrePaintTreeWalkTest() = default;
  ~SoftNavigationPrePaintTreeWalkTest() override = default;

  SoftNavigationContext* CreateSoftNavigationContext() {
    auto* initial_event_timing = CreatePerformanceEventTimingForTest(
        event_type_names::kClick, base::TimeTicks::Now(), GetDocument().body(),
        GetDocument().domWindow());
    return MakeGarbageCollected<SoftNavigationContext>(
        *GetDocument().domWindow(), initial_event_timing);
  }

 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

TEST_F(SoftNavigationPrePaintTreeWalkTest,
       ShouldInheritSoftNavigationContextUpdate) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor' style='width: 100px; height: 100px;'>
      <div id='target' style='width: 100px; height: 100px;'>
        <div id='descendant' style='width: 100px; height: 100px;'>
          <div id='content' style='width: 100px; height: 100px;'>
            Content
          </div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  auto& ancestor = *GetLayoutObjectByElementId("ancestor");
  auto& target = *GetLayoutObjectByElementId("target");
  auto& descendant = *GetLayoutObjectByElementId("descendant");
  auto& content = *GetLayoutObjectByElementId("content");

  EXPECT_FALSE(SoftNavigationContextChanged(ancestor));
  EXPECT_FALSE(SoftNavigationContextChanged(target));
  EXPECT_FALSE(SoftNavigationContextChanged(descendant));
  EXPECT_FALSE(SoftNavigationContextChanged(content));

  EXPECT_FALSE(DescendantSoftNavigationContextChanged(ancestor));
  EXPECT_FALSE(DescendantSoftNavigationContextChanged(target));
  EXPECT_FALSE(DescendantSoftNavigationContextChanged(descendant));
  EXPECT_FALSE(DescendantSoftNavigationContextChanged(content));

  EXPECT_TRUE(ancestor.ShouldInheritSoftNavigationContext());
  EXPECT_TRUE(target.ShouldInheritSoftNavigationContext());
  EXPECT_TRUE(descendant.ShouldInheritSoftNavigationContext());
  EXPECT_TRUE(content.ShouldInheritSoftNavigationContext());

  auto* context = CreateSoftNavigationContext();
  SoftNavigationHeuristics* heuristics =
      GetDocument().domWindow()->GetSoftNavigationHeuristics();
  ASSERT_TRUE(heuristics);
  SoftNavigationPaintAttributionTracker* tracker =
      heuristics->GetPaintAttributionTracker();
  ASSERT_TRUE(tracker);
  tracker->MarkNodeAsDirectlyModified(target.GetNode(), context);

  EXPECT_FALSE(SoftNavigationContextChanged(ancestor));
  EXPECT_TRUE(SoftNavigationContextChanged(target));
  EXPECT_FALSE(SoftNavigationContextChanged(descendant));
  EXPECT_FALSE(SoftNavigationContextChanged(content));

  EXPECT_TRUE(DescendantSoftNavigationContextChanged(ancestor));
  EXPECT_FALSE(DescendantSoftNavigationContextChanged(target));
  EXPECT_FALSE(DescendantSoftNavigationContextChanged(descendant));
  EXPECT_FALSE(DescendantSoftNavigationContextChanged(content));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(SoftNavigationContextChanged(ancestor));
  EXPECT_FALSE(SoftNavigationContextChanged(target));
  EXPECT_FALSE(SoftNavigationContextChanged(descendant));
  EXPECT_FALSE(SoftNavigationContextChanged(content));

  EXPECT_FALSE(DescendantSoftNavigationContextChanged(ancestor));
  EXPECT_FALSE(DescendantSoftNavigationContextChanged(target));
  EXPECT_FALSE(DescendantSoftNavigationContextChanged(descendant));
  EXPECT_FALSE(DescendantSoftNavigationContextChanged(content));

  EXPECT_TRUE(ancestor.ShouldInheritSoftNavigationContext());
  EXPECT_FALSE(target.ShouldInheritSoftNavigationContext());
  EXPECT_TRUE(descendant.ShouldInheritSoftNavigationContext());
  EXPECT_TRUE(content.ShouldInheritSoftNavigationContext());

  EXPECT_TRUE(tracker->IsAttributable(content.GetNode(), context));
}

class ContainerTimingPrePaintTreeWalkTest : public RenderingTest {
 public:
  ContainerTimingPrePaintTreeWalkTest() = default;
  ~ContainerTimingPrePaintTreeWalkTest() override = default;

  ContainerTimingPaintAttributionTracker* GetTracker() {
    return ContainerTiming::From(*GetDocument().domWindow())
        .PaintAttributionTracker();
  }

 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  ScopedContainerTimingForTest scoped_feature_{true};
};

// Mirrors ShouldInheritSoftNavigationContextUpdate: verifies the pre-paint walk
// maintains the per-LayoutObject container-timing dirty bits and the
// ShouldInheritContainerTimingRoot cache, and that adding a containertiming
// attribute re-attributes the subtree through the real walk.
TEST_F(ContainerTimingPrePaintTreeWalkTest,
       ShouldInheritContainerTimingRootUpdate) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor' style='width: 100px; height: 100px;'>
      <div id='target' style='width: 100px; height: 100px;'>
        <div id='descendant' style='width: 100px; height: 100px;'>
          <div id='content' style='width: 100px; height: 100px;'>
            Content
          </div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  auto& ancestor = *GetLayoutObjectByElementId("ancestor");
  auto& target = *GetLayoutObjectByElementId("target");
  auto& descendant = *GetLayoutObjectByElementId("descendant");
  auto& content = *GetLayoutObjectByElementId("content");

  // A clean walk with no containertiming roots leaves every node clean and
  // inheriting its (absent) ancestor root.
  EXPECT_FALSE(ContainerTimingChanged(ancestor));
  EXPECT_FALSE(ContainerTimingChanged(target));
  EXPECT_FALSE(ContainerTimingChanged(descendant));
  EXPECT_FALSE(ContainerTimingChanged(content));

  EXPECT_FALSE(DescendantContainerTimingChanged(ancestor));
  EXPECT_FALSE(DescendantContainerTimingChanged(target));
  EXPECT_FALSE(DescendantContainerTimingChanged(descendant));
  EXPECT_FALSE(DescendantContainerTimingChanged(content));

  EXPECT_TRUE(ancestor.ShouldInheritContainerTimingRoot());
  EXPECT_TRUE(target.ShouldInheritContainerTimingRoot());
  EXPECT_TRUE(descendant.ShouldInheritContainerTimingRoot());
  EXPECT_TRUE(content.ShouldInheritContainerTimingRoot());

  // Adding containertiming to #target dirties it and propagates the descendant
  // bit up to its ancestors, without touching the subtree below.
  auto* target_element = GetElementById("target");
  target_element->setAttribute(html_names::kContainertimingAttr,
                               AtomicString("target"));

  EXPECT_FALSE(ContainerTimingChanged(ancestor));
  EXPECT_TRUE(ContainerTimingChanged(target));
  EXPECT_FALSE(ContainerTimingChanged(descendant));
  EXPECT_FALSE(ContainerTimingChanged(content));

  EXPECT_TRUE(DescendantContainerTimingChanged(ancestor));
  EXPECT_FALSE(DescendantContainerTimingChanged(target));
  EXPECT_FALSE(DescendantContainerTimingChanged(descendant));
  EXPECT_FALSE(DescendantContainerTimingChanged(content));

  // The walk consumes the dirty bits and caches the inheritance decision:
  // #target becomes a root (does not inherit), its descendants inherit it.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ContainerTimingChanged(ancestor));
  EXPECT_FALSE(ContainerTimingChanged(target));
  EXPECT_FALSE(ContainerTimingChanged(descendant));
  EXPECT_FALSE(ContainerTimingChanged(content));

  EXPECT_FALSE(DescendantContainerTimingChanged(ancestor));
  EXPECT_FALSE(DescendantContainerTimingChanged(target));
  EXPECT_FALSE(DescendantContainerTimingChanged(descendant));
  EXPECT_FALSE(DescendantContainerTimingChanged(content));

  EXPECT_TRUE(ancestor.ShouldInheritContainerTimingRoot());
  EXPECT_FALSE(target.ShouldInheritContainerTimingRoot());
  EXPECT_TRUE(descendant.ShouldInheritContainerTimingRoot());
  EXPECT_TRUE(content.ShouldInheritContainerTimingRoot());

  // The tracker now attributes the text aggregation box (#content) to #target.
  ContainerTimingPaintAttributionTracker* tracker = GetTracker();
  ASSERT_TRUE(tracker);
  EXPECT_EQ(tracker->GetContainerRootFor(content.GetNode()), target_element);
}

}  // namespace blink
