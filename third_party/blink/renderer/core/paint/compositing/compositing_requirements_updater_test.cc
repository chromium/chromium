// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_requirements_updater.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

class CompositingRequirementsUpdaterTest : public RenderingTest {
 public:
  CompositingRequirementsUpdaterTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() final;
};

void CompositingRequirementsUpdaterTest::SetUp() {
  EnableCompositing();
  RenderingTest::SetUp();
}

TEST_F(CompositingRequirementsUpdaterTest,
       NoOverlapReasonForNonSelfPaintingLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
       overflow: auto;
       width: 100px;
       height: 100px;
       margin-top: -50px;
     }
    </style>
    <div style="position: relative; width: 500px; height: 300px;
        will-change: transform"></div>
    <div id=target></div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_FALSE(target->GetCompositingReasons());

  // Now make |target| self-painting.
  GetDocument().getElementById("target")->setAttribute(html_names::kStyleAttr,
                                                       "position: relative");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(CompositingReason::kOverlap, target->GetCompositingReasons());
}

// This test sets up a situation where a squashed PaintLayer loses its
// backing, but does not change visual rect. Therefore the compositing system
// must invalidate it because of change of backing.
TEST_F(CompositingRequirementsUpdaterTest,
       NeedsLayerAssignmentAfterSquashingRemoval) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0
      }
      #target {
        width: 100px; height: 100px; backface-visibility: hidden
      }
      div {
        width: 100px; height: 100px;
        position: absolute;
        background: lightblue;
        top: 0px;
      }
    </style>
    <div id=target></div>
    <div id=squashed></div>
  )HTML");

  PaintLayer* squashed = GetPaintLayerByElementId("squashed");
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());

  GetDocument().View()->SetTracksRasterInvalidations(true);

  GetDocument().getElementById("target")->setAttribute(html_names::kStyleAttr,
                                                       "display: none");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(kNotComposited, squashed->GetCompositingState());
  auto* tracking = GetDocument()
                       .View()
                       ->GetLayoutView()
                       ->Layer()
                       ->GraphicsLayerBacking()
                       ->GetRasterInvalidationTracking();
  EXPECT_TRUE(tracking->HasInvalidations());

  EXPECT_EQ(IntRect(0, 0, 100, 100), tracking->Invalidations()[0].rect);
}

TEST_F(CompositingRequirementsUpdaterTest, NonTrivial3DTransforms) {
  ScopedCSSIndependentTransformPropertiesForTest feature_scope(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="3d-transform" style="transform: translate3d(1px, 1px, 1px);"></div>
    <div id="2d-transform" style="transform: translate3d(1px, 1px, 0px);"></div>
    <div id="3d-transform-translate-z" style="transform:translateZ(1px);"></div>
    <div id="2d-transform-translate-z" style="transform:translateZ(0px);"></div>
    <div id="2d-transform-translate-x" style="transform:translateX(1px);"></div>
    <div id="3d-transform-rot-x" style="transform: rotateX(1deg);"></div>
    <div id="2d-transform-rot-x" style="transform: rotateX(0deg);"></div>
    <div id="2d-transform-rot-z" style="transform: rotateZ(1deg);"></div>
    <div id="3d-rotation-y" style="rotate: 0 1 0 1deg;"></div>
    <div id="2d-rotation-y" style="rotate: 0 1 0 0deg;"></div>
    <div id="2d-rotation-z" style="rotate: 0 0 1 1deg;"></div>
    <div id="3d-translation" style="translate: 0px 0px 1px;"></div>
    <div id="2d-translation" style="translate: 1px 1px 0px;"></div>
    <div id="3d-scale" style="scale: 2 2 2;"></div>
    <div id="2d-scale" style="scale: 2 2 1;"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const auto* transform_3d = GetLayoutBoxByElementId("3d-transform");
  EXPECT_TRUE(transform_3d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_TRUE(CompositingReason::k3DTransform &
              transform_3d->Layer()->GetCompositingReasons());
  const auto* transform_2d = GetLayoutBoxByElementId("2d-transform");
  EXPECT_FALSE(transform_2d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_TRUE(CompositingReason::kTrivial3DTransform &
              transform_2d->Layer()->GetCompositingReasons());

  const auto* transform_3d_translate_z =
      GetLayoutBoxByElementId("3d-transform-translate-z");
  EXPECT_TRUE(
      transform_3d_translate_z->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_TRUE(CompositingReason::k3DTransform &
              transform_3d_translate_z->Layer()->GetCompositingReasons());
  const auto* transform_2d_translate_z =
      GetLayoutBoxByElementId("2d-transform-translate-z");
  EXPECT_FALSE(
      transform_2d_translate_z->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_TRUE(CompositingReason::kTrivial3DTransform &
              transform_2d_translate_z->Layer()->GetCompositingReasons());
  const auto* transform_2d_translate_x =
      GetLayoutBoxByElementId("2d-transform-translate-x");
  EXPECT_FALSE(
      transform_2d_translate_x->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_FALSE(transform_2d_translate_x->Layer()->GetCompositingReasons());

  const auto* xform_rot_x_3d = GetLayoutBoxByElementId("3d-transform-rot-x");
  EXPECT_TRUE(xform_rot_x_3d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_TRUE(CompositingReason::k3DTransform &
              xform_rot_x_3d->Layer()->GetCompositingReasons());
  const auto* xform_rot_x_2d = GetLayoutBoxByElementId("2d-transform-rot-x");
  EXPECT_FALSE(xform_rot_x_2d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_TRUE(CompositingReason::kTrivial3DTransform &
              xform_rot_x_2d->Layer()->GetCompositingReasons());
  const auto* xform_rot_z_2d = GetLayoutBoxByElementId("2d-transform-rot-z");
  EXPECT_FALSE(xform_rot_z_2d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_FALSE(xform_rot_z_2d->Layer()->GetCompositingReasons());

  const auto* rotation_y_3d = GetLayoutBoxByElementId("3d-rotation-y");
  EXPECT_TRUE(rotation_y_3d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_TRUE(CompositingReason::k3DTransform &
              rotation_y_3d->Layer()->GetCompositingReasons());
  const auto* rotation_y_2d = GetLayoutBoxByElementId("2d-rotation-y");
  EXPECT_FALSE(rotation_y_2d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_TRUE(CompositingReason::kTrivial3DTransform &
              rotation_y_2d->Layer()->GetCompositingReasons());
  const auto* rotation_z_2d = GetLayoutBoxByElementId("2d-rotation-z");
  EXPECT_FALSE(rotation_z_2d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_FALSE(rotation_z_2d->Layer()->GetCompositingReasons());

  const auto* translation_3d = GetLayoutBoxByElementId("3d-translation");
  EXPECT_TRUE(translation_3d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_TRUE(CompositingReason::k3DTransform &
              translation_3d->Layer()->GetCompositingReasons());
  const auto* translation_2d = GetLayoutBoxByElementId("2d-translation");
  EXPECT_FALSE(translation_2d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_FALSE(translation_2d->Layer()->GetCompositingReasons());

  const auto* scale_3d = GetLayoutBoxByElementId("3d-scale");
  EXPECT_TRUE(scale_3d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_TRUE(CompositingReason::k3DTransform &
              scale_3d->Layer()->GetCompositingReasons());
  const auto* scale_2d = GetLayoutBoxByElementId("2d-scale");
  EXPECT_FALSE(scale_2d->StyleRef().HasNonTrivial3DTransformOperation());
  EXPECT_FALSE(scale_2d->Layer()->GetCompositingReasons());
}

TEST_F(CompositingRequirementsUpdaterTest,
       DontPromotePerspectiveOnlyTransform) {
  ScopedCSSIndependentTransformPropertiesForTest feature_scope(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="perspective-no-3d-descendant"
        style="transform:perspective(1px) scale(2)">
      <div id="transform2d" style="transform:translate(1px, 2px);"></div>
    </div>
    <div id="perspective-with-3d-descendant"
        style="transform:perspective(1px) scale(2)">
      <div id="3d-descendant" style="rotate: 0 1 0 1deg;"></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  // Perspective with descendant with only a 2d transform should not be layered
  // (neither should the descendant).
  EXPECT_FALSE(GetPaintLayerByElementId("perspective-no-3d-descendant")
                   ->GetCompositingState());
  EXPECT_FALSE(
      GetPaintLayerByElementId("transform2d")->GetCompositingReasons());

  // Both the perspective and 3d descendant should be layered, the former for
  // flattening purposes, as it contains 3d transformed content.
  EXPECT_EQ(CompositingReason::kPerspectiveWith3DDescendants,
            GetPaintLayerByElementId("perspective-with-3d-descendant")
                ->GetCompositingReasons());
  EXPECT_EQ(CompositingReason::k3DTransform,
            GetPaintLayerByElementId("3d-descendant")->GetCompositingReasons());
}

}  // namespace blink
