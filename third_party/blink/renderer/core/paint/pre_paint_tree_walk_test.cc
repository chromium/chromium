// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
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

 protected:
  PaintLayer* GetPaintLayerByElementId(const char* id) {
    return ToLayoutBoxModelObject(GetLayoutObjectByElementId(id))->Layer();
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }
};

INSTANTIATE_PAINT_TEST_CASE_P(PrePaintTreeWalkTest);

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithBorderInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #transformed { transform: translate(100px, 100px); }
      .border { border: 10px solid black; }
    </style>
    <div id='transformed'></div>
  )HTML");

  auto* transformed_element = GetDocument().getElementById("transformed");
  const auto* transformed_properties =
      transformed_element->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(100, 100),
            transformed_properties->Transform()->Matrix());

  // Artifically change the transform node.
  const_cast<ObjectPaintProperties*>(transformed_properties)->ClearTransform();
  EXPECT_EQ(nullptr, transformed_properties->Transform());

  // Cause a paint invalidation.
  transformed_element->setAttribute(HTMLNames::classAttr, "border");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Should have changed back.
  EXPECT_EQ(TransformationMatrix().Translate(100, 100),
            transformed_properties->Transform()->Matrix());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithFrameScroll) {
  SetBodyInnerHTML("<style> body { height: 10000px; } </style>");
  EXPECT_EQ(TransformationMatrix().Translate(0, 0),
            FrameScrollTranslation()->Matrix());

  // Cause a scroll invalidation and ensure the translation is updated.
  GetDocument().domWindow()->scrollTo(0, 100);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(TransformationMatrix().Translate(0, -100),
            FrameScrollTranslation()->Matrix());
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

  auto* transformed_element = GetDocument().getElementById("transformed");
  const auto* transformed_properties =
      transformed_element->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(100, 100),
            transformed_properties->Transform()->Matrix());

  // Invalidate the CSS transform property.
  transformed_element->setAttribute(HTMLNames::classAttr, "transformB");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // The transform should have changed.
  EXPECT_EQ(TransformationMatrix().Translate(200, 200),
            transformed_properties->Transform()->Matrix());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithOpacityInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .opacityA { opacity: 0.9; }
      .opacityB { opacity: 0.4; }
    </style>
    <div id='transparent' class='opacityA'></div>
  )HTML");

  auto* transparent_element = GetDocument().getElementById("transparent");
  const auto* transparent_properties =
      transparent_element->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(0.9f, transparent_properties->Effect()->Opacity());

  // Invalidate the opacity property.
  transparent_element->setAttribute(HTMLNames::classAttr, "opacityB");
  GetDocument().View()->UpdateAllLifecyclePhases();

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

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->NeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  parent->setAttribute(HTMLNames::classAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->NeedsRepaint());
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

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->NeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  parent->setAttribute(HTMLNames::classAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->NeedsRepaint());
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

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->NeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  // This changes clips for absolute-positioned descendants of "child" but not
  // normal-position ones, which are already clipped to 50x50.
  parent->setAttribute(HTMLNames::classAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->NeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, ClearSubsequenceCachingClipChangePosFixed) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .clip { overflow: hidden }
    </style>
    <div id='parent' style='transform: translateZ(0); width: 100px;
      height: 100px; trans'>
      <div id='child' style='overflow: hidden; z-index: 0;
          position: absolute; width: 50px; height: 50px'>
        content
      </div>
    </div>
  )HTML");

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->NeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  // This changes clips for absolute-positioned descendants of "child" but not
  // normal-position ones, which are already clipped to 50x50.
  parent->setAttribute(HTMLNames::classAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->NeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, VisualRectClipForceSubtree) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent { height: 75px; position: relative; width: 100px; }
    </style>
    <div id='parent' style='height: 100px;'>
      <div id='child' style='overflow: hidden; width: 100%; height: 100%;
          position: relative'>
        <div>
          <div id='grandchild' style='width: 50px; height: 200px; '>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  auto* grandchild = GetLayoutObjectByElementId("grandchild");

  GetDocument().getElementById("parent")->removeAttribute("style");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(200, grandchild->FirstFragment().VisualRect().Height());
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

  auto* target = GetDocument().getElementById("target");
  auto* target_object = ToLayoutBoxModelObject(target->GetLayoutObject());
  target->setAttribute(HTMLNames::styleAttr, "border-radius: 5px");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(target_object->Layer()->NeedsRepaint());
  // And should not trigger any assert failure.
  GetDocument().View()->UpdateAllLifecyclePhases();
}

namespace {
class PrePaintTreeWalkMockEventListener final : public EventListener {
 public:
  PrePaintTreeWalkMockEventListener() : EventListener(kCPPEventListenerType) {}

  bool operator==(const EventListener& other) const final {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event*) final {}
};
}  // namespace

TEST_P(PrePaintTreeWalkTest, InsideBlockingTouchEventHandlerUpdate) {
  ScopedPaintTouchActionRectsForTest enable_paint_touch_action_rects(true);
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor' style='width: 100px; height: 100px;'>
      <div id='handler' style='width: 100px; height: 100px;'>
        <div id='descendant' style='width: 100px; height: 100px;'>
        </div>
      </div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  auto& ancestor = *GetLayoutObjectByElementId("ancestor");
  auto& handler = *GetLayoutObjectByElementId("handler");
  auto& descendant = *GetLayoutObjectByElementId("descendant");

  EXPECT_FALSE(ancestor.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(handler.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveWhitelistedTouchActionChanged());

  EXPECT_FALSE(ancestor.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(handler.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveWhitelistedTouchActionChanged());

  EXPECT_FALSE(ancestor.InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler.InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant.InsideBlockingTouchEventHandler());

  PrePaintTreeWalkMockEventListener* callback =
      new PrePaintTreeWalkMockEventListener();
  auto* handler_element = GetDocument().getElementById("handler");
  handler_element->addEventListener(EventTypeNames::touchstart, callback);

  EXPECT_FALSE(ancestor.EffectiveWhitelistedTouchActionChanged());
  EXPECT_TRUE(handler.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveWhitelistedTouchActionChanged());

  EXPECT_TRUE(ancestor.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(handler.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveWhitelistedTouchActionChanged());

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(ancestor.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(handler.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveWhitelistedTouchActionChanged());

  EXPECT_FALSE(ancestor.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(handler.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveWhitelistedTouchActionChanged());

  EXPECT_FALSE(ancestor.InsideBlockingTouchEventHandler());
  EXPECT_TRUE(handler.InsideBlockingTouchEventHandler());
  EXPECT_TRUE(descendant.InsideBlockingTouchEventHandler());
}

TEST_P(PrePaintTreeWalkTest, EffectiveTouchActionStyleUpdate) {
  ScopedPaintTouchActionRectsForTest enable_paint_touch_action_rects(true);
  SetBodyInnerHTML(R"HTML(
    <style> .touchaction { touch-action: none; } </style>
    <div id='ancestor' style='width: 100px; height: 100px;'>
      <div id='touchaction' style='width: 100px; height: 100px;'>
        <div id='descendant' style='width: 100px; height: 100px;'>
        </div>
      </div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  auto& ancestor = *GetLayoutObjectByElementId("ancestor");
  auto& touchaction = *GetLayoutObjectByElementId("touchaction");
  auto& descendant = *GetLayoutObjectByElementId("descendant");

  EXPECT_FALSE(ancestor.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(touchaction.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(ancestor.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(touchaction.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveWhitelistedTouchActionChanged());

  GetDocument()
      .getElementById("touchaction")
      ->setAttribute(HTMLNames::classAttr, "touchaction");
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_FALSE(ancestor.EffectiveWhitelistedTouchActionChanged());
  EXPECT_TRUE(touchaction.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveWhitelistedTouchActionChanged());
  EXPECT_TRUE(ancestor.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(touchaction.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveWhitelistedTouchActionChanged());

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(ancestor.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(touchaction.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(ancestor.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(touchaction.DescendantEffectiveWhitelistedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveWhitelistedTouchActionChanged());
}

TEST_P(PrePaintTreeWalkTest, ClipChangesDoNotCauseVisualRectUpdates) {
  ScopedPaintTouchActionRectsForTest enable_paint_touch_action_rects(true);
  SetBodyInnerHTML(R"HTML(
    <style> #parent { width: 100px; height: 100px; overflow: hidden; } </style>
    <div id='parent'>
      <div id='child' style='width: 100px; height: 200px;'>
      </div>
    </div>
  )HTML");

  GetDocument().getElementById("parent")->setAttribute(HTMLNames::styleAttr,
                                                       "border-radius: 5px");

  GetDocument().View()->UpdateAllLifecyclePhases();
  auto& parent = *GetLayoutObjectByElementId("parent");
  auto& child = *GetLayoutObjectByElementId("child");

  // Cause the child to go down the prepaint path but without on its own
  // requiring a tree builder context.
  child.SetShouldCheckForPaintInvalidationWithoutGeometryChange();

  EXPECT_EQ(100, parent.FirstFragment().VisualRect().Width());
  EXPECT_EQ(100, parent.FirstFragment().VisualRect().Height());
  EXPECT_EQ(100, child.FirstFragment().VisualRect().Width());
  EXPECT_EQ(200, child.FirstFragment().VisualRect().Height());

  // Cause the child clip to change without changing paint property tree
  // topology.
  GetDocument().getElementById("parent")->setAttribute(HTMLNames::styleAttr,
                                                       "border-radius: 6px");

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(100, parent.FirstFragment().VisualRect().Width());
  EXPECT_EQ(100, parent.FirstFragment().VisualRect().Height());
  EXPECT_EQ(100, child.FirstFragment().VisualRect().Width());
  EXPECT_EQ(200, child.FirstFragment().VisualRect().Height());
}

}  // namespace blink
