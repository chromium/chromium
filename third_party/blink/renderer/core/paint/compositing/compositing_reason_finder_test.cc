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

#define EXPECT_REASONS(expect, actual)                        \
  EXPECT_EQ(expect, actual)                                   \
      << " expected: " << CompositingReason::ToString(expect) \
      << " actual: " << CompositingReason::ToString(actual)

INSTANTIATE_PAINT_TEST_SUITE_P(CompositingReasonFinderTest);

TEST_P(CompositingReasonFinderTest, PromoteTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(0)'></div>
  )HTML");

  EXPECT_REASONS(CompositingReason::kTrivial3DTransform,
                 CompositingReasonFinder::DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, PromoteNonTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(1px)'></div>
  )HTML");

  EXPECT_REASONS(CompositingReason::k3DTransform,
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
  EXPECT_REASONS(default_overscroll_type == OverscrollType::kTransform
                     ? CompositingReason::kFixedPosition |
                           CompositingReason::kUndoOverscroll
                     : CompositingReason::kNone,
                 CompositingReasonFinder::DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("fixedDiv")));

  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kNone);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_REASONS(CompositingReason::kNone,
                 CompositingReasonFinder::DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("fixedDiv")));

  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kTransform);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_REASONS(
      CompositingReason::kFixedPosition | CompositingReason::kUndoOverscroll,
      CompositingReasonFinder::DirectReasonsForPaintProperties(
          *GetLayoutObjectByElementId("fixedDiv")));
}

// Tests that an anchored-positioned fixpos element should overscroll if the
// anchor cab be overscrolled, so that it keeps "attached" to the anchor.
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
  EXPECT_REASONS(
      CompositingReason::kFixedPosition | CompositingReason::kAnchorPosition,
      CompositingReasonFinder::DirectReasonsForPaintProperties(
          *GetLayoutObjectByElementId("target")));

  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kTransform);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_REASONS(
      CompositingReason::kFixedPosition | CompositingReason::kAnchorPosition,
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
  EXPECT_REASONS(
      CompositingReason::kFixedPosition | CompositingReason::kAnchorPosition,
      CompositingReasonFinder::DirectReasonsForPaintProperties(
          *GetLayoutObjectByElementId("target")));

  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kTransform);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_REASONS(CompositingReason::kFixedPosition |
                     CompositingReason::kAnchorPosition |
                     CompositingReason::kUndoOverscroll,
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

  EXPECT_REASONS(CompositingReason::kStickyPosition,
                 CompositingReasonFinder::DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("sticky-top")));
  EXPECT_REASONS(CompositingReason::kNone,
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

  EXPECT_REASONS(CompositingReason::kStickyPosition,
                 CompositingReasonFinder::DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("sticky-scrolling")));

  EXPECT_REASONS(CompositingReason::kNone,
                 CompositingReasonFinder::DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("sticky-no-scrolling")));

  EXPECT_REASONS(CompositingReason::kStickyPosition,
                 CompositingReasonFinder::DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("overflow-hidden-scrolling")));

  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::DirectReasonsForPaintProperties(
          *GetLayoutObjectByElementId("overflow-hidden-no-scrolling")));

  EXPECT_REASONS(CompositingReason::kNone,
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

  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  CompositingReasons expected_reason = CompositingReason::kNone;

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentTransformAnimation(true);
  object->SetStyle(builder.TakeStyle());
  if (supports_transform_animation)
    expected_reason |= CompositingReason::kActiveTransformAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentScaleAnimation(true);
  object->SetStyle(builder.TakeStyle());
  if (supports_transform_animation)
    expected_reason |= CompositingReason::kActiveScaleAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentRotateAnimation(true);
  object->SetStyle(builder.TakeStyle());
  if (supports_transform_animation)
    expected_reason |= CompositingReason::kActiveRotateAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentTranslateAnimation(true);
  object->SetStyle(builder.TakeStyle());
  if (supports_transform_animation)
    expected_reason |= CompositingReason::kActiveTranslateAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentOpacityAnimation(true);
  object->SetStyle(builder.TakeStyle());
  expected_reason |= CompositingReason::kActiveOpacityAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentFilterAnimation(true);
  object->SetStyle(builder.TakeStyle());
  expected_reason |= CompositingReason::kActiveFilterAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  builder = ComputedStyleBuilder(object->StyleRef());
  builder.SetHasCurrentBackdropFilterAnimation(true);
  object->SetStyle(builder.TakeStyle());
  expected_reason |= CompositingReason::kActiveBackdropFilterAnimation;
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
  EXPECT_REASONS(CompositingReason::kNone,
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
  EXPECT_REASONS(CompositingReason::kIFrame,
                 CompositingReasonFinder::DirectReasonsForPaintProperties(
                     *iframe_layout_view));

  // Make the iframe contents scrollable.
  iframe->contentDocument()->body()->setAttribute(
      html_names::kStyleAttr, AtomicString("height: 2000px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_REASONS(CompositingReason::kIFrame,
                 CompositingReasonFinder::DirectReasonsForPaintProperties(
                     *iframe_layout_view));
  EXPECT_TRUE(CompositingReasonFinder::ShouldForcePreferCompositingToLCDText(
      *iframe_layout_view, CompositingReason::kIFrame));
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

  EXPECT_REASONS(CompositingReason::kBackfaceInvisibility3DAncestor |
                     CompositingReason::kTransform3DSceneLeaf,
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

  EXPECT_REASONS(CompositingReason::kBackfaceInvisibility3DAncestor,
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

  EXPECT_REASONS(CompositingReason::kBackfaceInvisibility3DAncestor,
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

  EXPECT_REASONS(CompositingReason::kBackfaceInvisibility3DAncestor |
                     CompositingReason::kTransform3DSceneLeaf,
                 CompositingReasonFinder::DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("intermediate")));
  EXPECT_REASONS(CompositingReason::kNone,
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

  EXPECT_REASONS(CompositingReason::kNone,
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

  EXPECT_REASONS(CompositingReason::kNone,
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
      CompositingReason::kWillChangeOpacity,
      CompositingReasonFinder::DirectReasonsForPaintProperties(*svg_text));
  auto* text = svg_text->SlowFirstChild();
  ASSERT_TRUE(text->IsText());
  EXPECT_REASONS(
      CompositingReason::kNone,
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
  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::DirectReasonsForPaintProperties(*defs));

  auto* text = GetLayoutObjectByElementId("text");
  EXPECT_REASONS(
      CompositingReason::kActiveTransformAnimation,
      CompositingReasonFinder::DirectReasonsForPaintProperties(*text));

  auto* text_content = text->SlowFirstChild();
  ASSERT_TRUE(text_content->IsText());
  EXPECT_EQ(
      CompositingReason::kNone,
      CompositingReasonFinder::DirectReasonsForPaintProperties(*text_content));

  auto* tspan = GetLayoutObjectByElementId("tspan");
  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::DirectReasonsForPaintProperties(*tspan));

  auto* tspan_content = tspan->SlowFirstChild();
  ASSERT_TRUE(tspan_content->IsText());
  EXPECT_EQ(
      CompositingReason::kNone,
      CompositingReasonFinder::DirectReasonsForPaintProperties(*tspan_content));

  auto* feBlend = GetLayoutObjectByElementId("feBlend");
  EXPECT_REASONS(
      CompositingReason::kNone,
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
      *target, CompositingReason::kNone));
  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::DirectReasonsForPaintProperties(*target));

  GetDocument()
      .getElementById(AtomicString("target"))
      ->RemoveInlineStyleProperty(CSSPropertyID::kWillChange);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(CompositingReasonFinder::ShouldForcePreferCompositingToLCDText(
      *target, CompositingReason::kNone));
  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::DirectReasonsForPaintProperties(*target));
}

}  // namespace blink
