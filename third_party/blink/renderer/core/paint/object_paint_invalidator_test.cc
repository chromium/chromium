// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

class ObjectPaintInvalidatorTest : public RenderingTest {
 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  static void ValidateDisplayItemClient(const DisplayItemClient* client) {
    client->Validate();
  }

  static bool IsValidDisplayItemClient(const DisplayItemClient* client) {
    return client->IsValid();
  }
};

using ::testing::ElementsAre;

TEST_F(ObjectPaintInvalidatorTest,
       TraverseNonCompositingDescendantsInPaintOrder) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>div { width: 10px; height: 10px; background-color: green;
    }</style>
    <div id='container' style='position: fixed'>
      <div id='normal-child'></div>
      <div id='stacked-child' style='position: relative'></div>
      <div id='composited-stacking-context' style='will-change: transform'>
        <div id='normal-child-of-composited-stacking-context'></div>
        <div id='stacked-child-of-composited-stacking-context'
    style='position: relative'></div>
      </div>
      <div id='composited-non-stacking-context' style='backface-visibility:
    hidden'>
        <div id='normal-child-of-composited-non-stacking-context'></div>
        <div id='stacked-child-of-composited-non-stacking-context'
    style='position: relative'></div>
        <div
    id='non-stacked-layered-child-of-composited-non-stacking-context'
    style='overflow: scroll'></div>
      </div>
    </div>
  )HTML");

  auto* container = GetLayoutObjectByElementId("container");
  auto* normal_child = GetLayoutObjectByElementId("normal-child");
  auto* stacked_child = GetLayoutObjectByElementId("stacked-child");
  auto* composited_stacking_context =
      GetLayoutObjectByElementId("composited-stacking-context");
  auto* normal_child_of_composited_stacking_context =
      GetLayoutObjectByElementId("normal-child-of-composited-stacking-context");
  auto* stacked_child_of_composited_stacking_context =
      GetLayoutObjectByElementId(
          "stacked-child-of-composited-stacking-context");
  auto* composited_non_stacking_context =
      GetLayoutObjectByElementId("composited-non-stacking-context");
  auto* normal_child_of_composited_non_stacking_context =
      GetLayoutObjectByElementId(
          "normal-child-of-composited-non-stacking-context");
  auto* stacked_child_of_composited_non_stacking_context =
      GetLayoutObjectByElementId(
          "stacked-child-of-composited-non-stacking-context");
  auto* non_stacked_layered_child_of_composited_non_stacking_context =
      GetLayoutObjectByElementId(
          "non-stacked-layered-child-of-composited-non-stacking-context");

  ValidateDisplayItemClient(container);
  ValidateDisplayItemClient(normal_child);
  ValidateDisplayItemClient(stacked_child);
  ValidateDisplayItemClient(composited_stacking_context);
  ValidateDisplayItemClient(normal_child_of_composited_stacking_context);
  ValidateDisplayItemClient(stacked_child_of_composited_stacking_context);
  ValidateDisplayItemClient(composited_non_stacking_context);
  ValidateDisplayItemClient(normal_child_of_composited_non_stacking_context);
  ValidateDisplayItemClient(stacked_child_of_composited_non_stacking_context);
  ValidateDisplayItemClient(
      non_stacked_layered_child_of_composited_non_stacking_context);

  ObjectPaintInvalidator(*container)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);

  EXPECT_FALSE(IsValidDisplayItemClient(container));
  EXPECT_FALSE(IsValidDisplayItemClient(normal_child));
  EXPECT_FALSE(IsValidDisplayItemClient(stacked_child));
  EXPECT_TRUE(IsValidDisplayItemClient(composited_stacking_context));
  EXPECT_TRUE(
      IsValidDisplayItemClient(normal_child_of_composited_stacking_context));
  EXPECT_TRUE(
      IsValidDisplayItemClient(stacked_child_of_composited_stacking_context));
  EXPECT_TRUE(IsValidDisplayItemClient(composited_non_stacking_context));
  EXPECT_TRUE(IsValidDisplayItemClient(
      normal_child_of_composited_non_stacking_context));
  EXPECT_FALSE(IsValidDisplayItemClient(
      stacked_child_of_composited_non_stacking_context));
  EXPECT_TRUE(IsValidDisplayItemClient(
      non_stacked_layered_child_of_composited_non_stacking_context));
}

TEST_F(ObjectPaintInvalidatorTest, TraverseFloatUnderCompositedInline) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>* { background: blue; }</style>
    <div id='compositedContainer' style='position: relative;
        will-change: transform'>
      <div id='containingBlock' style='position: relative'>
        <span id='span' style='position: relative; will-change: transform'>
          TEXT
          <div id='target' style='float: right'>FLOAT</div>
        </span>
      </div>
    </div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  auto* containing_block = GetLayoutObjectByElementId("containingBlock");
  auto* containing_block_layer =
      ToLayoutBoxModelObject(containing_block)->Layer();
  auto* composited_container =
      GetLayoutObjectByElementId("compositedContainer");
  auto* composited_container_layer =
      ToLayoutBoxModelObject(composited_container)->Layer();
  auto* span = GetLayoutObjectByElementId("span");
  auto* span_layer = ToLayoutBoxModelObject(span)->Layer();
  auto* text = span->SlowFirstChild();
  auto fragments = NGPaintFragment::InlineFragmentsFor(span);

  EXPECT_TRUE(span->IsPaintInvalidationContainer());
  EXPECT_TRUE(span->StyleRef().IsStackingContext());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, &target->ContainerForPaintInvalidation());
    EXPECT_EQ(span_layer, target->PaintingLayer());
  } else {
    EXPECT_EQ(composited_container, &target->ContainerForPaintInvalidation());
    EXPECT_EQ(containing_block_layer, target->PaintingLayer());
  }

  ValidateDisplayItemClient(target);
  ValidateDisplayItemClient(containing_block);
  ValidateDisplayItemClient(composited_container);
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    for (auto* fragment : fragments)
      ValidateDisplayItemClient(fragment);
  } else {
    ValidateDisplayItemClient(span);
    ValidateDisplayItemClient(text);
  }

  // Traversing from target should mark needsRepaint on correct layers.
  EXPECT_FALSE(containing_block_layer->SelfNeedsRepaint());
  EXPECT_FALSE(composited_container_layer->DescendantNeedsRepaint());
  ObjectPaintInvalidator(*target)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_FALSE(containing_block_layer->SelfOrDescendantNeedsRepaint());
    EXPECT_FALSE(composited_container_layer->SelfOrDescendantNeedsRepaint());
    EXPECT_TRUE(span_layer->SelfNeedsRepaint());
  } else {
    EXPECT_TRUE(containing_block_layer->SelfNeedsRepaint());
    EXPECT_FALSE(containing_block_layer->DescendantNeedsRepaint());
    EXPECT_FALSE(composited_container_layer->SelfNeedsRepaint());
    EXPECT_TRUE(composited_container_layer->DescendantNeedsRepaint());
    EXPECT_FALSE(span_layer->SelfNeedsRepaint());
  }
  EXPECT_FALSE(IsValidDisplayItemClient(target));
  ValidateDisplayItemClient(target);
  EXPECT_TRUE(IsValidDisplayItemClient(containing_block));
  EXPECT_TRUE(IsValidDisplayItemClient(composited_container));
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    for (auto* fragment : fragments)
      EXPECT_TRUE(IsValidDisplayItemClient(fragment));
  } else {
    EXPECT_TRUE(IsValidDisplayItemClient(span));
    EXPECT_TRUE(IsValidDisplayItemClient(text));
  }

  composited_container_layer->ClearNeedsRepaintRecursively();

  // Traversing from span should mark needsRepaint on correct layers for target.
  ValidateDisplayItemClient(target);
  EXPECT_FALSE(containing_block_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(composited_container_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(span_layer->SelfOrDescendantNeedsRepaint());
  ObjectPaintInvalidator(*span)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_FALSE(containing_block_layer->SelfOrDescendantNeedsRepaint());
    EXPECT_FALSE(composited_container_layer->SelfOrDescendantNeedsRepaint());
  } else {
    EXPECT_TRUE(containing_block_layer->SelfNeedsRepaint());
    EXPECT_FALSE(containing_block_layer->DescendantNeedsRepaint());
    EXPECT_FALSE(composited_container_layer->SelfNeedsRepaint());
    EXPECT_TRUE(composited_container_layer->DescendantNeedsRepaint());
  }
  EXPECT_TRUE(span_layer->SelfNeedsRepaint());

  EXPECT_FALSE(IsValidDisplayItemClient(target));
  ValidateDisplayItemClient(target);
  EXPECT_TRUE(IsValidDisplayItemClient(containing_block));
  EXPECT_TRUE(IsValidDisplayItemClient(composited_container));
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    for (auto* fragment : fragments) {
      EXPECT_FALSE(IsValidDisplayItemClient(fragment));
      ValidateDisplayItemClient(fragment);
    }
  } else {
    EXPECT_FALSE(IsValidDisplayItemClient(span));
    ValidateDisplayItemClient(span);
    EXPECT_FALSE(IsValidDisplayItemClient(text));
    ValidateDisplayItemClient(text);
  }

  composited_container_layer->ClearNeedsRepaintRecursively();

  // Traversing from compositedContainer should not reach target.
  EXPECT_FALSE(containing_block_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(composited_container_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(span_layer->SelfOrDescendantNeedsRepaint());
  ObjectPaintInvalidator(*composited_container)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(containing_block_layer->SelfNeedsRepaint());
  EXPECT_TRUE(composited_container_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(span_layer->SelfNeedsRepaint());

  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    EXPECT_TRUE(IsValidDisplayItemClient(target));
  else
    EXPECT_FALSE(IsValidDisplayItemClient(target));
  EXPECT_FALSE(IsValidDisplayItemClient(containing_block));
  EXPECT_FALSE(IsValidDisplayItemClient(composited_container));
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    for (auto* fragment : fragments)
      EXPECT_TRUE(IsValidDisplayItemClient(fragment));
  } else {
    EXPECT_TRUE(IsValidDisplayItemClient(span));
    EXPECT_TRUE(IsValidDisplayItemClient(text));
  }
}

TEST_F(ObjectPaintInvalidatorTest, TraverseStackedFloatUnderCompositedInline) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <span id='span' style='position: relative; will-change: transform'>
      <div id='target' style='position: relative; float: right'></div>
    </span>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  auto* target_layer = ToLayoutBoxModelObject(target)->Layer();
  auto* span = GetLayoutObjectByElementId("span");
  auto* span_layer = ToLayoutBoxModelObject(span)->Layer();
  auto* text = span->SlowFirstChild();
  auto fragments = NGPaintFragment::InlineFragmentsFor(span);

  EXPECT_TRUE(span->IsPaintInvalidationContainer());
  EXPECT_TRUE(span->StyleRef().IsStackingContext());
  EXPECT_EQ(span, &target->ContainerForPaintInvalidation());
  EXPECT_EQ(target_layer, target->PaintingLayer());

  ValidateDisplayItemClient(target);
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    for (auto* fragment : fragments)
      ValidateDisplayItemClient(fragment);
  } else {
    ValidateDisplayItemClient(span);
    ValidateDisplayItemClient(text);
  }

  // Traversing from span should reach target.
  EXPECT_FALSE(span_layer->SelfNeedsRepaint());
  ObjectPaintInvalidator(*span)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(span_layer->SelfNeedsRepaint());

  EXPECT_FALSE(IsValidDisplayItemClient(target));
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    for (auto* fragment : fragments)
      EXPECT_FALSE(IsValidDisplayItemClient(fragment));
  } else {
    EXPECT_FALSE(IsValidDisplayItemClient(span));
    EXPECT_FALSE(IsValidDisplayItemClient(text));
  }
}

TEST_F(ObjectPaintInvalidatorTest, InvalidatePaintRectangle) {
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px; background: blue'>"
      "</div>");

  GetDocument().View()->SetTracksPaintInvalidations(true);

  auto* target = GetLayoutObjectByElementId("target");
  target->InvalidatePaintRectangle(PhysicalRect(10, 10, 50, 50));
  EXPECT_EQ(PhysicalRect(10, 10, 50, 50),
            target->PartialInvalidationLocalRect());
  target->InvalidatePaintRectangle(PhysicalRect(30, 30, 60, 60));
  EXPECT_EQ(PhysicalRect(10, 10, 80, 80),
            target->PartialInvalidationLocalRect());
  EXPECT_TRUE(target->ShouldCheckForPaintInvalidation());

  EXPECT_TRUE(IsValidDisplayItemClient(target));
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(PhysicalRect(), target->PartialInvalidationLocalRect());
  EXPECT_EQ(IntRect(18, 18, 80, 80), target->PartialInvalidationVisualRect());
  EXPECT_FALSE(IsValidDisplayItemClient(target));

  target->InvalidatePaintRectangle(PhysicalRect(30, 30, 50, 80));
  EXPECT_EQ(PhysicalRect(30, 30, 50, 80),
            target->PartialInvalidationLocalRect());
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // PartialInvalidationVisualRect should accumulate until painting.
  EXPECT_EQ(IntRect(18, 18, 80, 100), target->PartialInvalidationVisualRect());

  UpdateAllLifecyclePhasesForTest();
  const auto& raster_invalidations = GetLayoutView()
                                         .Layer()
                                         ->GraphicsLayerBacking()
                                         ->GetRasterInvalidationTracking()
                                         ->Invalidations();
  ASSERT_EQ(1u, raster_invalidations.size());
  EXPECT_EQ(IntRect(18, 18, 80, 100), raster_invalidations[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kRectangle,
            raster_invalidations[0].reason);

  EXPECT_TRUE(IsValidDisplayItemClient(target));
}

TEST_F(ObjectPaintInvalidatorTest, Selection) {
  SetBodyInnerHTML("<img id='target' style='width: 100px; height: 100px'>");
  auto* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(IntRect(), target->SelectionVisualRect());

  // Add selection.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  GetDocument().GetFrame()->Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  const auto* graphics_layer = GetLayoutView().Layer()->GraphicsLayerBacking();
  const auto* invalidations =
      &graphics_layer->GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(1u, invalidations->size());
  EXPECT_EQ(IntRect(8, 8, 100, 100), (*invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kSelection, (*invalidations)[0].reason);
  EXPECT_EQ(IntRect(8, 8, 100, 100), target->SelectionVisualRect());
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Simulate a change without full invalidation or selection change.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->SetShouldCheckForPaintInvalidation();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(graphics_layer->GetRasterInvalidationTracking()
                  ->Invalidations()
                  .IsEmpty());
  EXPECT_EQ(IntRect(8, 8, 100, 100), target->SelectionVisualRect());
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Remove selection.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  GetDocument().GetFrame()->Selection().Clear();
  UpdateAllLifecyclePhasesForTest();
  invalidations =
      &graphics_layer->GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(1u, invalidations->size());
  EXPECT_EQ(IntRect(8, 8, 100, 100), (*invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kSelection, (*invalidations)[0].reason);
  EXPECT_EQ(IntRect(), target->SelectionVisualRect());
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

// Passes if it does not crash.
TEST_F(ObjectPaintInvalidatorTest, ZeroWidthForeignObject) {
  SetBodyInnerHTML(R"HTML(
    <svg style="backface-visibility: hidden;">
      <foreignObject width=0 height=50>
        <div style="position: relative">test</div>
      </foreignObject>
    </svg>
  )HTML");
}

}  // namespace blink
