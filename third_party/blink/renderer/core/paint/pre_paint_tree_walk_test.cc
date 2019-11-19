// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
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

  auto* transformed_element = GetDocument().getElementById("transformed");
  const auto* transformed_properties =
      transformed_element->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(FloatSize(100, 100),
            transformed_properties->Transform()->Translation2D());

  // Artifically change the transform node.
  const_cast<ObjectPaintProperties*>(transformed_properties)->ClearTransform();
  EXPECT_EQ(nullptr, transformed_properties->Transform());

  // Cause a paint invalidation.
  transformed_element->setAttribute(html_names::kClassAttr, "border");
  UpdateAllLifecyclePhasesForTest();

  // Should have changed back.
  EXPECT_EQ(FloatSize(100, 100),
            transformed_properties->Transform()->Translation2D());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithFrameScroll) {
  SetBodyInnerHTML("<style> body { height: 10000px; } </style>");
  EXPECT_TRUE(FrameScrollTranslation()->IsIdentity());

  // Cause a scroll invalidation and ensure the translation is updated.
  GetDocument().domWindow()->scrollTo(0, 100);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(FloatSize(0, -100), FrameScrollTranslation()->Translation2D());
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
  EXPECT_EQ(FloatSize(100, 100),
            transformed_properties->Transform()->Translation2D());

  // Invalidate the CSS transform property.
  transformed_element->setAttribute(html_names::kClassAttr, "transformB");
  UpdateAllLifecyclePhasesForTest();

  // The transform should have changed.
  EXPECT_EQ(FloatSize(200, 200),
            transformed_properties->Transform()->Translation2D());
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
  transparent_element->setAttribute(html_names::kClassAttr, "opacityB");
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

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  parent->setAttribute(html_names::kClassAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

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

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  parent->setAttribute(html_names::kClassAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

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

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  // This changes clips for absolute-positioned descendants of "child" but not
  // normal-position ones, which are already clipped to 50x50.
  parent->setAttribute(html_names::kClassAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

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

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  // This changes clips for absolute-positioned descendants of "child" but not
  // normal-position ones, which are already clipped to 50x50.
  parent->setAttribute(html_names::kClassAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->SelfNeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, ClipChangeRepaintsDescendants) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent { height: 75px; position: relative; width: 100px; }
      #child { overflow: hidden; width: 10%; height: 100%; position: relative; }
      #greatgrandchild {
        width: 5px; height: 5px; z-index: 100; position: relative;
      }
    </style>
    <div id='parent' style='height: 100px;'>
      <div id='child'>
        <div id='grandchild'>
          <div id='greatgrandchild'></div>
        </div>
      </div>
    </div>
  )HTML");

  GetDocument().getElementById("parent")->removeAttribute("style");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  auto* greatgrandchild = GetLayoutObjectByElementId("greatgrandchild");
  auto* paint_layer = ToLayoutBoxModelObject(greatgrandchild)->Layer();
  EXPECT_TRUE(paint_layer->SelfNeedsRepaint());
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
  UpdateAllLifecyclePhasesForTest();

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
  target->setAttribute(html_names::kStyleAttr, "border-radius: 5px");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
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
  auto* handler_element = GetDocument().getElementById("handler");
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
      .getElementById("touchaction")
      ->setAttribute(html_names::kClassAttr, "touchaction");
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_FALSE(ancestor.EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(touchaction.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(ancestor.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(touchaction.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveAllowedTouchActionChanged());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(touchaction.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(ancestor.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(touchaction.DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant.DescendantEffectiveAllowedTouchActionChanged());
}

TEST_P(PrePaintTreeWalkTest, ClipChangesDoNotCauseVisualRectUpdates) {
  SetBodyInnerHTML(R"HTML(
    <style> #parent { width: 100px; height: 100px; overflow: hidden; } </style>
    <div id='parent'>
      <div id='child' style='width: 100px; height: 200px;'>
      </div>
    </div>
  )HTML");

  GetDocument().getElementById("parent")->setAttribute(html_names::kStyleAttr,
                                                       "border-radius: 5px");

  UpdateAllLifecyclePhasesForTest();
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
  GetDocument().getElementById("parent")->setAttribute(html_names::kStyleAttr,
                                                       "border-radius: 6px");

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(100, parent.FirstFragment().VisualRect().Width());
  EXPECT_EQ(100, parent.FirstFragment().VisualRect().Height());
  EXPECT_EQ(100, child.FirstFragment().VisualRect().Width());
  EXPECT_EQ(200, child.FirstFragment().VisualRect().Height());
}

}  // namespace blink
