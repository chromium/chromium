// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_requirements_updater.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
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

class CompositingRequirementsUpdaterSimTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }
};

TEST_F(CompositingRequirementsUpdaterSimTest,
       StaleCompositingStateInThrottledFrame) {
  SimRequest top_resource("https://example.com/top.html", "text/html");
  SimRequest middle_resource("https://cross-origin.com/middle.html",
                             "text/html");
  SimRequest bottom_resource("https://cross-origin.com/bottom.html",
                             "text/html");

  LoadURL("https://example.com/top.html");
  top_resource.Complete(R"HTML(
    <div id='spacer'></div>
    <iframe id='middle' src='https://cross-origin.com/middle.html'></iframe>
  )HTML");
  middle_resource.Complete(R"HTML(
    <iframe id='bottom' src='bottom.html'></iframe>
  )HTML");
  bottom_resource.Complete(R"HTML(
    <div id='composited' style='will-change:transform'>Hello, world!</div>
  )HTML");

  LocalFrame& middle_frame =
      *To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  LocalFrame& bottom_frame = *To<LocalFrame>(middle_frame.Tree().FirstChild());
  middle_frame.View()->BeginLifecycleUpdates();
  bottom_frame.View()->BeginLifecycleUpdates();
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(bottom_frame.View()->ShouldThrottleRenderingForTest());
  LayoutEmbeddedContent* bottom_owner = bottom_frame.OwnerLayoutObject();
  EXPECT_TRUE(bottom_owner->ContentDocumentContainsGraphicsLayer());
  EXPECT_TRUE(bottom_owner->Layer()->HasCompositingDescendant());

  // Move iframe offscreen to throttle it. Compositing status shouldn't change.
  Element* spacer = GetDocument().getElementById("spacer");
  spacer->setAttribute(html_names::kStyleAttr, "height:2000px");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(middle_frame.View()->ShouldThrottleRenderingForTest());
  ASSERT_TRUE(bottom_frame.View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(bottom_owner->ContentDocumentContainsGraphicsLayer());
  EXPECT_TRUE(bottom_owner->Layer()->HasCompositingDescendant());

  // Remove direct compositing reason from iframe content, but add
  // position:relative so it still has a PaintLayer and won't force a
  // compositing update.
  Element* composited =
      bottom_frame.GetDocument()->getElementById("composited");
  composited->setAttribute(html_names::kStyleAttr, "position:relative");

  // Force a lifecycle update up to pre-paint clean; compositing inputs will be
  // updated, but not compositing assignments. This imitates what would happen
  // if a new IntersectionObservation is created inside a throttled frame. This
  // should not affect final compositing state.
  GetDocument().View()->ForceUpdateViewportIntersections();
  EXPECT_TRUE(bottom_owner->ContentDocumentContainsGraphicsLayer());
  EXPECT_TRUE(bottom_owner->Layer()->HasCompositingDescendant());

  // Force a full, throttled lifecycle update. Compositing state in the bottom
  // frame will remain stale; compositing state in the middle frame will be
  // based on the stale state of the iframe.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      middle_frame.ContentLayoutObject()->Layer()->GetCompositingReasons() |
      CompositingReason::kRoot);
  EXPECT_TRUE(bottom_owner->ContentDocumentContainsGraphicsLayer());
  EXPECT_TRUE(bottom_owner->Layer()->HasCompositingDescendant());

  // Move the iframe back on screen and run two lifecycle updates to unthrottle
  // it and update compositing.
  spacer->setAttribute(html_names::kStyleAttr, "");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(middle_frame.View()->ShouldThrottleRenderingForTest());
  ASSERT_FALSE(bottom_frame.View()->ShouldThrottleRenderingForTest());
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(bottom_owner->ContentDocumentContainsGraphicsLayer());
  EXPECT_FALSE(bottom_owner->Layer()->HasCompositingDescendant());
}

}  // namespace blink
