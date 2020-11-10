// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"

#include "third_party/blink/renderer/core/animation/animatable.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {
class PaintLayerCompositorTest : public RenderingTest {
 public:
  PaintLayerCompositorTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};
}  // namespace

TEST_F(PaintLayerCompositorTest, AdvancingToCompositingInputsClean) {
  SetBodyInnerHTML("<div id='box' style='position: relative'></div>");

  PaintLayer* box_layer = GetPaintLayerByElementId("box");
  ASSERT_TRUE(box_layer);
  EXPECT_FALSE(box_layer->NeedsCompositingInputsUpdate());

  box_layer->SetNeedsCompositingInputsUpdate();

  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
            GetDocument().Lifecycle().GetState());
  EXPECT_FALSE(box_layer->NeedsCompositingInputsUpdate());

  GetDocument().View()->SetNeedsLayout();
  EXPECT_TRUE(GetDocument().View()->NeedsLayout());
}

TEST_F(PaintLayerCompositorTest,
       CompositingInputsCleanDoesNotTriggerAnimations) {
  SetBodyInnerHTML(R"HTML(
    <style>@keyframes fadeOut { from { opacity: 1; } to { opacity: 0; } }
    .animate { animation: fadeOut 2s; }</style>
    <div id='box'></div>
    <div id='otherBox'></div>
  )HTML");

  Element* box = GetDocument().getElementById("box");
  Element* otherBox = GetDocument().getElementById("otherBox");
  ASSERT_TRUE(box);
  ASSERT_TRUE(otherBox);

  box->setAttribute("class", "animate", ASSERT_NO_EXCEPTION);

  // Update the lifecycle to CompositingInputsClean. This should not start the
  // animation lifecycle.
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
            GetDocument().Lifecycle().GetState());

  otherBox->setAttribute("class", "animate", ASSERT_NO_EXCEPTION);

  // Now run the rest of the lifecycle. Because both 'box' and 'otherBox' were
  // given animations separated only by a lifecycle update to
  // CompositingInputsClean, they should both be started in the same lifecycle
  // and as such grouped together.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            GetDocument().Lifecycle().GetState());

  HeapVector<Member<Animation>> boxAnimations = box->getAnimations();
  HeapVector<Member<Animation>> otherBoxAnimations = otherBox->getAnimations();

  EXPECT_EQ(1ul, boxAnimations.size());
  EXPECT_EQ(1ul, otherBoxAnimations.size());
  EXPECT_EQ(boxAnimations.front()->CompositorGroup(),
            otherBoxAnimations.front()->CompositorGroup());
}

TEST_F(PaintLayerCompositorTest, CompositingInputsUpdateStopsContainStrict) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      div {
        position: relative;
      }
      #wrapper {
        contain: strict;
        }
    </style>
    <div id='wrapper'>
      <div id='target'></div>
    </div>
  )HTML");

  PaintLayer* wrapper = GetPaintLayerByElementId("wrapper");
  PaintLayer* target = GetPaintLayerByElementId("target");
  EXPECT_FALSE(wrapper->NeedsCompositingInputsUpdate());
  EXPECT_FALSE(target->NeedsCompositingInputsUpdate());

  target->SetNeedsCompositingInputsUpdate();
  EXPECT_FALSE(wrapper->NeedsCompositingInputsUpdate());
  EXPECT_TRUE(target->NeedsCompositingInputsUpdate());

  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
            GetDocument().Lifecycle().GetState());
  EXPECT_FALSE(wrapper->NeedsCompositingInputsUpdate());
  EXPECT_FALSE(target->NeedsCompositingInputsUpdate());
}

TEST_F(PaintLayerCompositorTest, SubframeRebuildGraphicsLayers) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML("<iframe src='http://test.com'></iframe>");
  SetChildFrameHTML(
      "<div id='target' style='will-change: opacity; opacity: 0.5'></div>");

  UpdateAllLifecyclePhasesForTest();
  auto* child_layout_view = ChildDocument().GetLayoutView();
  auto* child_root_graphics_layer =
      child_layout_view->Layer()->GraphicsLayerBacking(child_layout_view);
  ASSERT_TRUE(child_root_graphics_layer);
  EXPECT_EQ(GetLayoutView().Layer()->GraphicsLayerBacking(),
            child_root_graphics_layer->Parent());

  // This simulates that the subframe rebuilds GraphicsLayer tree, while the
  // main frame doesn't have any compositing flags set. The root GraphicsLayer
  // of the subframe should be hooked up in the GraphicsLayer tree correctly.
  child_root_graphics_layer->RemoveFromParent();
  child_layout_view->Compositor()->SetNeedsCompositingUpdate(
      kCompositingUpdateRebuildTree);
  GetLayoutView().Compositor()->UpdateAssignmentsIfNeededRecursive(
      DocumentLifecycle::kCompositingAssignmentsClean);
  ASSERT_EQ(
      child_root_graphics_layer,
      child_layout_view->Layer()->GraphicsLayerBacking(child_layout_view));
  EXPECT_EQ(GetLayoutView().Layer()->GraphicsLayerBacking(),
            child_root_graphics_layer->Parent());
}

}  // namespace blink
