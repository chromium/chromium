// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CompositingReasonFinderTest : public RenderingTest,
                                    public PaintTestConfigurations {
 public:
  CompositingReasonFinderTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  void SimulateFrame() {
    // Advance time by 100 ms.
    auto new_time = GetAnimationClock().CurrentTime() + base::Milliseconds(100);
    GetPage().Animator().ServiceScriptedAnimations(new_time);
  }

  void CheckCompositingReasonsForAnimation(bool supports_transform_animation);
};

INSTANTIATE_PAINT_TEST_SUITE_P(CompositingReasonFinderTest);

TEST_P(CompositingReasonFinderTest, PromoteTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(0)'></div>
  )HTML");

  EXPECT_EQ(CompositingReasons{CompositingReason::kTrivial3DTransform},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, PromoteNonTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(1px)'></div>
  )HTML");

  EXPECT_EQ(CompositingReasons{CompositingReason::k3DTransform},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, UndoOverscroll) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .fixedDivStyle {
      position: fixed;
      width: 100px;
      height: 100px;
      border: 1px solid;
    }
    </style>
    <body style="background-image: linear-gradient(grey, yellow);">
      <div id="fixedDiv" class='fixedDivStyle'></div>
    </body>
  )HTML");

  auto& visual_viewport = GetDocument().GetPage()->GetVisualViewport();
  auto default_overscroll_type = visual_viewport.GetOverscrollType();
  CompositingReasons expected_reasons;
  if (default_overscroll_type == OverscrollType::kTransform) {
    expected_reasons = {CompositingReason::kFixedPosition,
                        CompositingReason::kUndoOverscroll};
  }
  EXPECT_EQ(expected_reasons,
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("fixedDiv")));

  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kNone);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("fixedDiv")));

  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kTransform);
  UpdateAllLifecyclePhasesForTest();
  expected_reasons = {CompositingReason::kFixedPosition,
                      CompositingReason::kUndoOverscroll};
  EXPECT_EQ(expected_reasons,
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("fixedDiv")));
}

// Tests that an anchored-positioned fixpos element should overscroll if the
// anchor can be overscrolled, so that it keeps "attached" to the anchor.
TEST_P(CompositingReasonFinderTest, FixedPosAnchorPosOverscroll) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { height: 200vh; }
      div { width: 100px; height: 100px; }
      #anchor { anchor-name: --a; position: absolute; background: orange; }
      #target { position-anchor: --a; top: anchor(top);
                position: fixed; background: lime; }
    </style>
    <div id="anchor"></div>
    <div id="target"></div>
  )HTML");

  // Need frame update to update `AnchorPositionScrollData`.
  SimulateFrame();
  UpdateAllLifecyclePhasesForTest();

  auto& visual_viewport = GetDocument().GetPage()->GetVisualViewport();
  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kNone);
  UpdateAllLifecyclePhasesForTest();
  CompositingReasons expected_reasons = {CompositingReason::kFixedPosition,
                                         CompositingReason::kAnchorPosition};
  EXPECT_EQ(expected_reasons,
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));

  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kTransform);
  CompositingReasons expected_reasons_with_overflow = {
      CompositingReason::kFixedPosition, CompositingReason::kAnchorPosition};
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(expected_reasons_with_overflow,
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));

  // Adjust the body so that it does not scroll, but is still affected by
  // elastic overscroll effects.
  GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                     AtomicString("height: 50vh"));
  UpdateAllLifecyclePhasesForTest();
  // When AnchorPositionAdjustmentWithoutOverflow is enabled, the behavior
  // should be the same as the non-overflow case because overflow effects are
  // the same regardless of actual scrollable overflow.
  auto expected_reasons_without_overflow = expected_reasons_with_overflow;
  if (!RuntimeEnabledFeatures::
          AnchorPositionAdjustmentWithoutOverflowEnabled()) {
    expected_reasons_without_overflow = {CompositingReason::kFixedPosition,
                                         CompositingReason::kUndoOverscroll};
  }
  EXPECT_EQ(expected_reasons_without_overflow,
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));
}

// Tests that an anchored-positioned fixpos element should not overscroll if
// the anchor does not overscroll.
TEST_P(CompositingReasonFinderTest, FixedPosAnchorPosUndoOverscroll) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { height: 200vh; }
      #scroller {
        position: fixed; overflow: scroll; width: 200px; height: 200px;
      }
      #anchor, #target { width: 100px; height: 100px; }
      #anchor { anchor-name: --a; position: absolute;
                top: 300px; background: orange; }
      #target { position-anchor: --a; top: anchor(top);
                position: fixed; background: lime; }
    </style>
    <div id="scroller">
      <div id="anchor"></div>
    </div>
    <div id="target"></div>
  )HTML");

  // Need frame update to update `AnchorPositionScrollData`.
  SimulateFrame();
  UpdateAllLifecyclePhasesForTest();

  auto& visual_viewport = GetDocument().GetPage()->GetVisualViewport();
  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kNone);
  UpdateAllLifecyclePhasesForTest();
  CompositingReasons expected_reasons = {CompositingReason::kFixedPosition,
                                         CompositingReason::kAnchorPosition};
  EXPECT_EQ(expected_reasons,
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));

  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kTransform);
  UpdateAllLifecyclePhasesForTest();
  expected_reasons = {CompositingReason::kFixedPosition,
                      CompositingReason::kAnchorPosition,
                      CompositingReason::kUndoOverscroll};
  EXPECT_EQ(expected_reasons,
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, OnlyAnchoredStickyPositionPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .scroller {contain: paint; width: 400px; height: 400px; overflow: auto;
    will-change: transform;}
    .sticky { position: sticky; width: 10px; height: 10px;}</style>
    <div class='scroller'>
      <div id='sticky-top' class='sticky' style='top: 0px;'></div>
      <div id='sticky-no-anchor' class='sticky'></div>
      <div style='height: 2000px;'></div>
    </div>
  )HTML");

  EXPECT_EQ(CompositingReasons{CompositingReason::kStickyPosition},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("sticky-top")));
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("sticky-no-anchor")));
}

TEST_P(CompositingReasonFinderTest, OnlyScrollingStickyPositionPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .scroller {
        width: 400px;
        height: 400px;
        overflow: auto;
        will-change: transform;
      }
      .sticky {
        position: sticky;
        top: 0;
        width: 10px;
        height: 10px;
      }
      .overflow-hidden {
        width: 400px;
        height: 400px;
        overflow: hidden;
        will-change: transform;
      }
    </style>
    <div class='scroller'>
      <div id='sticky-scrolling' class='sticky'></div>
      <div style='height: 2000px;'></div>
    </div>
    <div class='scroller'>
      <div id='sticky-no-scrolling' class='sticky'></div>
    </div>
    <div class='overflow-hidden'>
      <div id='overflow-hidden-scrolling' class='sticky'></div>
      <div style='height: 2000px;'></div>
    </div>
    <div class='overflow-hidden'>
      <div id='overflow-hidden-no-scrolling' class='sticky'></div>
    </div>
    <div style="position: fixed">
      <div id='under-fixed' class='sticky'></div>
    </div>
    < div style='height: 2000px;"></div>
  )HTML");

  EXPECT_EQ(CompositingReasons{CompositingReason::kStickyPosition},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("sticky-scrolling")));

  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("sticky-no-scrolling")));

  EXPECT_EQ(CompositingReasons{CompositingReason::kStickyPosition},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("overflow-hidden-scrolling")));

  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("overflow-hidden-no-scrolling")));

  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("under-fixed")));
}

void CompositingReasonFinderTest::CheckCompositingReasonsForAnimation(
    bool supports_transform_animation) {
  auto* object = GetLayoutObjectByElementId("target");
  ComputedStyleBuilder builder =
      GetDocument().GetStyleResolver().CreateComputedStyleBuilder();

  builder.SetSubtreeWillChangeContents(false);
  builder.SetHasCurrentTransformAnimation(false);
  builder.SetHasCurrentScaleAnimation(false);
  builder.SetHasCurrentRotateAnimation(false);
  builder.SetHasCurrentTranslateAnimation(false);
  builder.SetHasCurrentOpacityAnimation(false);
  builder.SetHasCurrentFilterAnimation(false);
  builder.SetHasCurrentBackdropFilterAnimation(false);
  object->SetStyle(builder.TakeStyle());

  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  CompositingReasons expected_reason;

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentTransformAnimation(true);
  object->SetStyle(builder.TakeStyle());
  if (supports_transform_animation)
    expected_reason.Put(CompositingReason::kActiveTransformAnimation);
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentScaleAnimation(true);
  object->SetStyle(builder.TakeStyle());
  if (supports_transform_animation)
    expected_reason.Put(CompositingReason::kActiveScaleAnimation);
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentRotateAnimation(true);
  object->SetStyle(builder.TakeStyle());
  if (supports_transform_animation)
    expected_reason.Put(CompositingReason::kActiveRotateAnimation);
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentTranslateAnimation(true);
  object->SetStyle(builder.TakeStyle());
  if (supports_transform_animation)
    expected_reason.Put(CompositingReason::kActiveTranslateAnimation);
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentOpacityAnimation(true);
  object->SetStyle(builder.TakeStyle());
  expected_reason.Put(CompositingReason::kActiveOpacityAnimation);
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentFilterAnimation(true);
  object->SetStyle(builder.TakeStyle());
  expected_reason.Put(CompositingReason::kActiveFilterAnimation);
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentBackdropFilterAnimation(true);
  object->SetStyle(builder.TakeStyle());
  expected_reason.Put(CompositingReason::kActiveBackdropFilterAnimation);
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));
}

TEST_P(CompositingReasonFinderTest, CompositingReasonsForAnimationBox) {
  SetBodyInnerHTML("<div id='target'>Target</div>");
  CheckCompositingReasonsForAnimation(/*supports_transform_animation*/ true);
}

TEST_P(CompositingReasonFinderTest, CompositingReasonsForAnimationInline) {
  SetBodyInnerHTML("<span id='target'>Target</span>");
  CheckCompositingReasonsForAnimation(/*supports_transform_animation*/ false);
}

TEST_P(CompositingReasonFinderTest, DontPromoteEmptyIframe) {
  SetPreferCompositingToLCDText(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe style="width:0; height:0; border: 0;" srcdoc="<!DOCTYPE html>"></iframe>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  ASSERT_TRUE(child_frame);
  LocalFrameView* child_frame_view = child_frame->View();
  ASSERT_TRUE(child_frame_view);
  EXPECT_FALSE(child_frame_view->CanThrottleRendering());
}

TEST_P(CompositingReasonFinderTest, PromoteCrossOriginIframe) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe id=iframe></iframe>
  )HTML");

  HTMLFrameOwnerElement* iframe = To<HTMLFrameOwnerElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  ASSERT_TRUE(iframe);
  iframe->contentDocument()->OverrideIsInitialEmptyDocument();
  To<LocalFrame>(iframe->ContentFrame())->View()->BeginLifecycleUpdates();
  ASSERT_FALSE(iframe->ContentFrame()->IsCrossOriginToNearestMainFrame());
  UpdateAllLifecyclePhasesForTest();
  LayoutView* iframe_layout_view =
      To<LocalFrame>(iframe->ContentFrame())->ContentLayoutObject();
  ASSERT_TRUE(iframe_layout_view);
  PaintLayer* iframe_layer = iframe_layout_view->Layer();
  ASSERT_TRUE(iframe_layer);
  EXPECT_FALSE(iframe_layer->GetScrollableArea()->UsesCompositedScrolling());
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *iframe_layout_view));

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe id=iframe sandbox></iframe>
  )HTML");
  iframe = To<HTMLFrameOwnerElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  iframe->contentDocument()->OverrideIsInitialEmptyDocument();
  To<LocalFrame>(iframe->ContentFrame())->View()->BeginLifecycleUpdates();
  UpdateAllLifecyclePhasesForTest();
  iframe_layout_view =
      To<LocalFrame>(iframe->ContentFrame())->ContentLayoutObject();
  iframe_layer = iframe_layout_view->Layer();
  ASSERT_TRUE(iframe_layer);
  ASSERT_TRUE(iframe->ContentFrame()->IsCrossOriginToNearestMainFrame());
  EXPECT_FALSE(iframe_layer->GetScrollableArea()->UsesCompositedScrolling());
  EXPECT_EQ(CompositingReasons{CompositingReason::kIFrame},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *iframe_layout_view));

  // Make the iframe contents scrollable.
  iframe->contentDocument()->body()->setAttribute(
      html_names::kStyleAttr, AtomicString("height: 2000px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CompositingReasons{CompositingReason::kIFrame},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *iframe_layout_view));
  EXPECT_TRUE(CompositingReasonFinder::ShouldForcePreferCompositingToLCDText(
      *iframe_layout_view, {CompositingReason::kIFrame}));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndPreserve3DAncestor) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div id=target></div>
    </div>
  )HTML");

  CompositingReasons expected_reasons = {
      CompositingReason::kBackfaceInvisibility3DAncestor,
      CompositingReason::kTransform3DSceneLeaf};
  EXPECT_EQ(expected_reasons,
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndPreserve3D) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div id=target style="transform-style: preserve-3d"></div>
    </div>
  )HTML");

  EXPECT_EQ(
      CompositingReasons{CompositingReason::kBackfaceInvisibility3DAncestor},
      CompositingReasonFinder::DirectReasonsForPaintProperties(
          *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndPreserve3DWithInterveningDiv) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div>
        <div id=target style="position: relative"></div>
      </div>
    </div>
  )HTML");

  EXPECT_EQ(
      CompositingReasons{CompositingReason::kBackfaceInvisibility3DAncestor},
      CompositingReasonFinder::DirectReasonsForPaintProperties(
          *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorWithInterveningStackingDiv) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div id=intermediate style="isolation: isolate">
        <div id=target style="position: relative"></div>
      </div>
    </div>
  )HTML");

  CompositingReasons expected_reasons = {
      CompositingReason::kBackfaceInvisibility3DAncestor,
      CompositingReason::kTransform3DSceneLeaf};
  EXPECT_EQ(expected_reasons,
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("intermediate")));
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndFlattening) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div style="backface-visibility: hidden;">
      <div id=target></div>
    </div>
  )HTML");

  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, CompositeWithBackfaceVisibility) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div id=target style="backface-visibility: hidden;">
      <div></div>
    </div>
  )HTML");

  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, CompositedSVGText) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <text id="text" style="will-change: opacity">Text</text>
    </svg>
  )HTML");

  auto* svg_text = GetLayoutObjectByElementId("text");
  EXPECT_EQ(
      CompositingReasons{CompositingReason::kWillChangeOpacity},
      CompositingReasonFinder::DirectReasonsForPaintProperties(*svg_text));
  auto* text = svg_text->SlowFirstChild();
  ASSERT_TRUE(text->IsText());
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(*text));
}

TEST_P(CompositingReasonFinderTest, NotSupportedTransformAnimationsOnSVG) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { animation: transformKeyframes 1s infinite; }
      @keyframes transformKeyframes {
        0% { transform: rotate(-5deg); }
        100% { transform: rotate(5deg); }
      }
    </style>
    <svg>
      <defs id="defs" />
      <text id="text">text content
        <tspan id="tspan">tspan content</tspan>
      </text>
      <filter>
        <feBlend id="feBlend"></feBlend>
      </filter>
    </svg>
  )HTML");

  auto* defs = GetLayoutObjectByElementId("defs");
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(*defs));

  auto* text = GetLayoutObjectByElementId("text");
  EXPECT_EQ(CompositingReasons{CompositingReason::kActiveTransformAnimation},
            CompositingReasonFinder::DirectReasonsForPaintProperties(*text));

  auto* text_content = text->SlowFirstChild();
  ASSERT_TRUE(text_content->IsText());
  EXPECT_EQ(
      CompositingReasons{},
      CompositingReasonFinder::DirectReasonsForPaintProperties(*text_content));

  auto* tspan = GetLayoutObjectByElementId("tspan");
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(*tspan));

  auto* tspan_content = tspan->SlowFirstChild();
  ASSERT_TRUE(tspan_content->IsText());
  EXPECT_EQ(
      CompositingReasons{},
      CompositingReasonFinder::DirectReasonsForPaintProperties(*tspan_content));

  auto* feBlend = GetLayoutObjectByElementId("feBlend");
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(*feBlend));
}

TEST_P(CompositingReasonFinderTest, WillChangeScrollPosition) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 100px; height: 100px; overflow: scroll;
                            will-change: scroll-position">
      <div style="height: 2000px"></div>
    </div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  EXPECT_TRUE(CompositingReasonFinder::ShouldForcePreferCompositingToLCDText(
      *target, {}));
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(*target));

  GetDocument()
      .getElementById(AtomicString("target"))
      ->RemoveInlineStyleProperty(CSSPropertyID::kWillChange);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(CompositingReasonFinder::ShouldForcePreferCompositingToLCDText(
      *target, {}));
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(*target));
}

TEST_P(CompositingReasonFinderTest, UnboundedElementCompositingReason) {
  ScopedUnboundedElementForTest unbounded_element_enabled(true);
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 100px; height: 100px;"></div>
  )HTML");

  auto* element = GetDocument().getElementById(AtomicString("target"));
  auto* html_element = To<HTMLElement>(element);

  // 1. No attribute, inactive: kNone
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));

  // 2. With attribute, but inactive: kNone
  html_element->setAttribute(html_names::kUnboundedAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));

  // 3. With attribute AND active: kUnboundedElement
  html_element->SetUnboundedElementActive(true);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CompositingReasons{CompositingReason::kUnboundedElement},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));

  // 4. Remove attribute, keep active: kNone
  html_element->removeAttribute(html_names::kUnboundedAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, CanvasChild) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <canvas id=canvas layoutsubtree>
      <div id=child style="width: 10px; height: 10px;">
       <div id=grandchild style="width: 10px; height: 10px;"
       </div>
      </div>
    </canvas>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* canvas = GetElementById("canvas");
  ASSERT_TRUE(canvas);
  LayoutObject* canvas_layout_object = canvas->GetLayoutObject();
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *canvas_layout_object));

  Element* child = GetElementById("child");
  ASSERT_TRUE(child);
  LayoutObject* child_layout_object = child->GetLayoutObject();
  EXPECT_EQ(CompositingReasons{CompositingReason::kCanvasChild},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *child_layout_object));

  Element* grandchild = GetElementById("grandchild");
  ASSERT_TRUE(grandchild);
  LayoutObject* grandchild_layout_object = grandchild->GetLayoutObject();
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *grandchild_layout_object));
}

TEST_P(CompositingReasonFinderTest, CanvasChildSlotted) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  GetDocument().GetSettings()->SetScriptEnabled(true);
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id=slotHost>
      <template shadowrootmode=open>
        <canvas layoutsubtree>
          <slot name="slot1"></slot>
        </canvas>
      </template>
      <div id=slotted slot="slot1">
        <p id=slotchild>Hello</p>
      </div>
    </div>
  )");
  UpdateAllLifecyclePhasesForTest();

  Element* slotted = GetElementById("slotted");
  ASSERT_TRUE(slotted);
  EXPECT_TRUE(slotted->IsInCanvasSubtree());
  EXPECT_TRUE(slotted->IsCanvasOrInCanvasSubtree());
  LayoutObject* layout_object = slotted->GetLayoutObject();
  ASSERT_TRUE(layout_object);
  EXPECT_EQ(
      CompositingReasons{CompositingReason::kCanvasChild},
      CompositingReasonFinder::DirectReasonsForPaintProperties(*layout_object));

  Element* slot_child = GetElementById("slotchild");
  ASSERT_TRUE(slot_child);
  EXPECT_TRUE(slot_child->IsInCanvasSubtree());
  EXPECT_TRUE(slot_child->IsCanvasOrInCanvasSubtree());
  LayoutObject* child_layout_object = slot_child->GetLayoutObject();
  ASSERT_TRUE(child_layout_object);
  EXPECT_EQ(CompositingReasons{},
            CompositingReasonFinder::DirectReasonsForPaintProperties(
                *child_layout_object));
}

}  // namespace blink
