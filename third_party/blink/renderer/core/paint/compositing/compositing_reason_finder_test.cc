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

namespace blink {

class CompositingReasonFinderTest : public RenderingTest {
 public:
  CompositingReasonFinderTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
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

class CompositingReasonFinderTestWithDoNotCompositeTrivial3D
    : public CompositingReasonFinderTest {
 public:
  CompositingReasonFinderTestWithDoNotCompositeTrivial3D() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kDoNotCompositeTrivial3D);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CompositingReasonFinderTestWithDoNotCompositeTrivial3D,
       DontPromoteTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(0)'></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  EXPECT_EQ(kNotComposited, paint_layer->GetCompositingState());
}

class CompositingReasonFinderTestWithCompositeTrivial3D
    : public CompositingReasonFinderTest {
 public:
  CompositingReasonFinderTestWithCompositeTrivial3D() {
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kDoNotCompositeTrivial3D);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CompositingReasonFinderTestWithCompositeTrivial3D, PromoteTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(0)'></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, PromoteNonTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(1px)'></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
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
            ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky-top"))
                ->Layer()
                ->GetCompositingState());
  EXPECT_EQ(kNotComposited, ToLayoutBoxModelObject(
                                GetLayoutObjectByElementId("sticky-no-anchor"))
                                ->Layer()
                                ->GetCompositingState());
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
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky-scrolling"))
          ->Layer()
          ->GetCompositingState());
  EXPECT_EQ(
      kNotComposited,
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky-no-scrolling"))
          ->Layer()
          ->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, CompositingReasonsForAnimation) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  style->SetSubtreeWillChangeContents(false);
  style->SetHasCurrentTransformAnimation(false);
  style->SetHasCurrentOpacityAnimation(false);
  style->SetHasCurrentFilterAnimation(false);
  style->SetHasCurrentBackdropFilterAnimation(false);
  EXPECT_EQ(CompositingReason::kNone,
            CompositingReasonFinder::CompositingReasonsForAnimation(*style));

  style->SetHasCurrentTransformAnimation(true);
  EXPECT_EQ(CompositingReason::kActiveTransformAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*style));

  style->SetHasCurrentOpacityAnimation(true);
  EXPECT_EQ(CompositingReason::kActiveTransformAnimation |
                CompositingReason::kActiveOpacityAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*style));

  style->SetHasCurrentFilterAnimation(true);
  EXPECT_EQ(CompositingReason::kActiveTransformAnimation |
                CompositingReason::kActiveOpacityAnimation |
                CompositingReason::kActiveFilterAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*style));

  style->SetHasCurrentBackdropFilterAnimation(true);
  EXPECT_EQ(CompositingReason::kActiveTransformAnimation |
                CompositingReason::kActiveOpacityAnimation |
                CompositingReason::kActiveFilterAnimation |
                CompositingReason::kActiveBackdropFilterAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*style));
  EXPECT_EQ(CompositingReason::kComboActiveAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*style));
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

  Element* iframe = GetDocument().getElementById("iframe");
  ASSERT_TRUE(iframe);
  PaintLayer* iframe_layer =
      ToLayoutBoxModelObject(iframe->GetLayoutObject())->Layer();
  ASSERT_TRUE(iframe_layer);
  ASSERT_FALSE(To<HTMLFrameOwnerElement>(iframe)
                   ->ContentFrame()
                   ->IsCrossOriginSubframe());
  EXPECT_EQ(kNotComposited, iframe_layer->DirectCompositingReasons());

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe id=iframe sandbox></iframe>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  iframe = GetDocument().getElementById("iframe");
  ASSERT_TRUE(iframe);
  iframe_layer = ToLayoutBoxModelObject(iframe->GetLayoutObject())->Layer();
  ASSERT_TRUE(iframe_layer);
  ASSERT_TRUE(To<HTMLFrameOwnerElement>(iframe)
                  ->ContentFrame()
                  ->IsCrossOriginSubframe());
  EXPECT_EQ(CompositingReason::kCrossOriginIframe,
            iframe_layer->DirectCompositingReasons());
}

}  // namespace blink
