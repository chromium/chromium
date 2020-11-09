// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

class CompositingReasonFinderTest : public RenderingTest {
 public:
  CompositingReasonFinderTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  void CheckCompositingReasonsForAnimation(bool supports_transform_animation);
};

TEST_F(CompositingReasonFinderTest, CompositingReasonDependencies) {
  EXPECT_FALSE(CompositingReason::kComboAllDirectNonStyleDeterminedReasons &
               (~CompositingReason::kComboAllDirectReasons));
  EXPECT_EQ(CompositingReason::kComboAllDirectReasons,
            CompositingReason::kComboAllDirectStyleDeterminedReasons |
                CompositingReason::kComboAllDirectNonStyleDeterminedReasons);
  EXPECT_FALSE(CompositingReason::kComboAllDirectNonStyleDeterminedReasons &
               CompositingReason::kComboAllStyleDeterminedReasons);
}

TEST_F(CompositingReasonFinderTest, PromoteTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(0)'></div>
  )HTML");

  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, PromoteNonTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(1px)'></div>
  )HTML");

  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
}

class CompositingReasonFinderTestLowEndPlatform
    : public TestingPlatformSupport {
 public:
  bool IsLowEndDevice() override { return true; }
};

TEST_F(CompositingReasonFinderTest, DontPromoteTrivial3DWithLowEndDevice) {
  ScopedTestingPlatformSupport<CompositingReasonFinderTestLowEndPlatform>
      platform;
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(0)'></div>
  )HTML");

  PaintLayer* paint_layer = GetPaintLayerByElementId("target");
  EXPECT_EQ(kNotComposited, paint_layer->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, OnlyAnchoredStickyPositionPromoted) {
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

  EXPECT_EQ(kPaintsIntoOwnBacking,
            GetPaintLayerByElementId("sticky-top")->GetCompositingState());
  EXPECT_EQ(
      kNotComposited,
      GetPaintLayerByElementId("sticky-no-anchor")->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, OnlyScrollingStickyPositionPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>.scroller {width: 400px; height: 400px; overflow: auto;
    will-change: transform;}
    .sticky { position: sticky; top: 0; width: 10px; height: 10px;}
    </style>
    <div class='scroller'>
      <div id='sticky-scrolling' class='sticky'></div>
      <div style='height: 2000px;'></div>
    </div>
    <div class='scroller'>
      <div id='sticky-no-scrolling' class='sticky'></div>
    </div>
  )HTML");

  EXPECT_EQ(
      kPaintsIntoOwnBacking,
      GetPaintLayerByElementId("sticky-scrolling")->GetCompositingState());
  EXPECT_EQ(
      kNotComposited,
      GetPaintLayerByElementId("sticky-no-scrolling")->GetCompositingState());
}

void CompositingReasonFinderTest::CheckCompositingReasonsForAnimation(
    bool supports_transform_animation) {
  auto* object = GetLayoutObjectByElementId("target");
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  style->SetSubtreeWillChangeContents(false);
  style->SetHasCurrentTransformAnimation(false);
  style->SetHasCurrentOpacityAnimation(false);
  style->SetHasCurrentFilterAnimation(false);
  style->SetHasCurrentBackdropFilterAnimation(false);
  object->SetStyle(style);

  EXPECT_EQ(CompositingReason::kNone,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  CompositingReasons expected_compositing_reason_for_transform_animation =
      supports_transform_animation
          ? CompositingReason::kActiveTransformAnimation
          : CompositingReason::kNone;

  style->SetHasCurrentTransformAnimation(true);
  EXPECT_EQ(expected_compositing_reason_for_transform_animation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentOpacityAnimation(true);
  EXPECT_EQ(expected_compositing_reason_for_transform_animation |
                CompositingReason::kActiveOpacityAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentFilterAnimation(true);
  EXPECT_EQ(expected_compositing_reason_for_transform_animation |
                CompositingReason::kActiveOpacityAnimation |
                CompositingReason::kActiveFilterAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentBackdropFilterAnimation(true);
  EXPECT_EQ(expected_compositing_reason_for_transform_animation |
                CompositingReason::kActiveOpacityAnimation |
                CompositingReason::kActiveFilterAnimation |
                CompositingReason::kActiveBackdropFilterAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));
}

TEST_F(CompositingReasonFinderTest, CompositingReasonsForAnimationBox) {
  SetBodyInnerHTML("<div id='target'>Target</div>");
  CheckCompositingReasonsForAnimation(/*supports_transform_animation*/ true);
}

TEST_F(CompositingReasonFinderTest, CompositingReasonsForAnimationInline) {
  SetBodyInnerHTML("<span id='target'>Target</span>");
  CheckCompositingReasonsForAnimation(/*supports_transform_animation*/ false);
}

TEST_F(CompositingReasonFinderTest, DontPromoteEmptyIframe) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);

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
  EXPECT_EQ(kNotComposited,
            child_frame_view->GetLayoutView()->Layer()->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, PromoteCrossOriginIframe) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(
      blink::features::kCompositeCrossOriginIframes, true);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe id=iframe></iframe>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  HTMLFrameOwnerElement* iframe =
      To<HTMLFrameOwnerElement>(GetDocument().getElementById("iframe"));
  ASSERT_TRUE(iframe);
  ASSERT_FALSE(iframe->ContentFrame()->IsCrossOriginToMainFrame());
  LayoutView* iframe_layout_view =
      To<LocalFrame>(iframe->ContentFrame())->ContentLayoutObject();
  ASSERT_TRUE(iframe_layout_view);
  PaintLayer* iframe_layer = iframe_layout_view->Layer();
  ASSERT_TRUE(iframe_layer);
  EXPECT_EQ(kNotComposited, iframe_layer->DirectCompositingReasons());

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe id=iframe sandbox></iframe>
  )HTML");
  iframe = To<HTMLFrameOwnerElement>(GetDocument().getElementById("iframe"));
  To<LocalFrame>(iframe->ContentFrame())
      ->GetDocument()
      ->OverrideIsInitialEmptyDocument();
  To<LocalFrame>(iframe->ContentFrame())->View()->BeginLifecycleUpdates();
  UpdateAllLifecyclePhasesForTest();
  iframe_layout_view =
      To<LocalFrame>(iframe->ContentFrame())->ContentLayoutObject();
  iframe_layer = iframe_layout_view->Layer();
  ASSERT_TRUE(iframe_layer);
  ASSERT_TRUE(iframe->ContentFrame()->IsCrossOriginToMainFrame());
  EXPECT_EQ(CompositingReason::kIFrame,
            iframe_layer->DirectCompositingReasons());
}

TEST_F(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndPreserve3D) {
  ScopedTransformInteropForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div id=target></div>
    </div>
  )HTML");

  PaintLayer* target_layer = GetPaintLayerByElementId("target");

  EXPECT_EQ(CompositingReason::kBackfaceInvisibility3DAncestor,
            target_layer->PotentialCompositingReasonsFromNonStyle());
  EXPECT_EQ(kPaintsIntoOwnBacking, target_layer->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndPreserve3DWithInterveningDiv) {
  ScopedTransformInteropForTest enabled(true);

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

  PaintLayer* target_layer = GetPaintLayerByElementId("target");

  EXPECT_EQ(CompositingReason::kBackfaceInvisibility3DAncestor,
            target_layer->PotentialCompositingReasonsFromNonStyle());
  EXPECT_EQ(kPaintsIntoOwnBacking, target_layer->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorWithInterveningStackingDiv) {
  ScopedTransformInteropForTest enabled(true);

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

  PaintLayer* intermediate_layer = GetPaintLayerByElementId("intermediate");
  PaintLayer* target_layer = GetPaintLayerByElementId("target");

  EXPECT_EQ(CompositingReason::kBackfaceInvisibility3DAncestor,
            intermediate_layer->PotentialCompositingReasonsFromNonStyle());
  EXPECT_EQ(kPaintsIntoOwnBacking, intermediate_layer->GetCompositingState());

  EXPECT_EQ(CompositingReason::kNone,
            target_layer->PotentialCompositingReasonsFromNonStyle());
  EXPECT_NE(kPaintsIntoOwnBacking, target_layer->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndFlattening) {
  ScopedTransformInteropForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div style="backface-visibility: hidden;">
      <div id=target></div>
    </div>
  )HTML");

  PaintLayer* target_layer = GetPaintLayerByElementId("target");

  EXPECT_EQ(CompositingReason::kNone,
            target_layer->PotentialCompositingReasonsFromNonStyle());
  EXPECT_NE(kPaintsIntoOwnBacking, target_layer->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, CompositeWithBackfaceVisibility) {
  ScopedTransformInteropForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div id=target style="backface-visibility: hidden;">
      <div></div>
    </div>
  )HTML");

  PaintLayer* target_layer = GetPaintLayerByElementId("target");

  EXPECT_EQ(CompositingReason::kNone,
            target_layer->PotentialCompositingReasonsFromNonStyle());
  EXPECT_EQ(kPaintsIntoOwnBacking, target_layer->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, CompositedSVGText) {
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
  EXPECT_EQ(CompositingReason::kNone,
            CompositingReasonFinder::DirectReasonsForPaintProperties(*text));
}

}  // namespace blink
