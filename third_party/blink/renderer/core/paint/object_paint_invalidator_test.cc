// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
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

TEST_F(ObjectPaintInvalidatorTest, TraverseNonCompositingDescendants) {
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
    style='overflow: scroll'>
          <div style="height:40px"></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* container = GetLayoutObjectByElementId("container");
  auto* container_layer = To<LayoutBoxModelObject>(container)->Layer();
  auto* stacked_child = GetLayoutObjectByElementId("stacked-child");
  auto* stacked_child_layer = To<LayoutBoxModelObject>(stacked_child)->Layer();
  auto* composited_stacking_context =
      GetLayoutObjectByElementId("composited-stacking-context");
  auto* composited_stacking_context_layer =
      To<LayoutBoxModelObject>(composited_stacking_context)->Layer();
  auto* stacked_child_of_composited_stacking_context =
      GetLayoutObjectByElementId(
          "stacked-child-of-composited-stacking-context");
  auto* stacked_child_of_composited_stacking_context_layer =
      To<LayoutBoxModelObject>(stacked_child_of_composited_stacking_context)
          ->Layer();
  auto* composited_non_stacking_context =
      GetLayoutObjectByElementId("composited-non-stacking-context");
  auto* composited_non_stacking_context_layer =
      To<LayoutBoxModelObject>(composited_non_stacking_context)->Layer();
  auto* stacked_child_of_composited_non_stacking_context =
      GetLayoutObjectByElementId(
          "stacked-child-of-composited-non-stacking-context");
  auto* stacked_child_of_composited_non_stacking_context_layer =
      To<LayoutBoxModelObject>(stacked_child_of_composited_non_stacking_context)
          ->Layer();
  auto* non_stacked_layered_child_of_composited_non_stacking_context =
      GetLayoutObjectByElementId(
          "non-stacked-layered-child-of-composited-non-stacking-context");
  auto* non_stacked_layered_child_of_composited_non_stacking_context_layer =
      To<LayoutBoxModelObject>(
          non_stacked_layered_child_of_composited_non_stacking_context)
          ->Layer();

  ObjectPaintInvalidator(*container)
      .InvalidatePaintIncludingNonCompositingDescendants();

  EXPECT_TRUE(container_layer->SelfNeedsRepaint());
  EXPECT_TRUE(stacked_child_layer->SelfNeedsRepaint());
  EXPECT_FALSE(composited_stacking_context_layer->SelfNeedsRepaint());
  EXPECT_FALSE(
      stacked_child_of_composited_stacking_context_layer->SelfNeedsRepaint());
  EXPECT_FALSE(composited_non_stacking_context_layer->SelfNeedsRepaint());
  EXPECT_TRUE(stacked_child_of_composited_non_stacking_context_layer
                  ->SelfNeedsRepaint());
  EXPECT_FALSE(
      non_stacked_layered_child_of_composited_non_stacking_context_layer
          ->SelfNeedsRepaint());
}

static const LayoutBoxModelObject& EnclosingCompositedContainer(
    const LayoutObject& layout_object) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  return layout_object.PaintingLayer()
      ->EnclosingLayerForPaintInvalidationCrossingFrameBoundaries()
      ->GetLayoutObject();
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
  auto* containing_block_layer = GetPaintLayerByElementId("containingBlock");
  auto* composited_container =
      GetLayoutObjectByElementId("compositedContainer");
  auto* composited_container_layer =
      To<LayoutBoxModelObject>(composited_container)->Layer();
  auto* span = GetLayoutObjectByElementId("span");
  auto* span_layer = To<LayoutBoxModelObject>(span)->Layer();

  EXPECT_TRUE(span->IsPaintInvalidationContainer());
  EXPECT_TRUE(span->IsStackingContext());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(span, &EnclosingCompositedContainer(*target));
    EXPECT_EQ(span_layer, target->PaintingLayer());
  } else {
    EXPECT_EQ(composited_container, &EnclosingCompositedContainer(*target));
    EXPECT_EQ(containing_block_layer, target->PaintingLayer());
  }

  // Traversing from target should mark needsRepaint on correct layers.
  EXPECT_FALSE(containing_block_layer->SelfNeedsRepaint());
  EXPECT_FALSE(composited_container_layer->DescendantNeedsRepaint());
  ObjectPaintInvalidator(*target)
      .InvalidatePaintIncludingNonCompositingDescendants();
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

  composited_container_layer->ClearNeedsRepaintRecursively();

  // Traversing from span should mark needsRepaint on correct layers for target.
  EXPECT_FALSE(containing_block_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(composited_container_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(span_layer->SelfOrDescendantNeedsRepaint());
  ObjectPaintInvalidator(*span)
      .InvalidatePaintIncludingNonCompositingDescendants();
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

  composited_container_layer->ClearNeedsRepaintRecursively();

  // Traversing from compositedContainer should not reach target.
  EXPECT_FALSE(containing_block_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(composited_container_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(span_layer->SelfOrDescendantNeedsRepaint());
  ObjectPaintInvalidator(*composited_container)
      .InvalidatePaintIncludingNonCompositingDescendants();
  EXPECT_TRUE(containing_block_layer->SelfNeedsRepaint());
  EXPECT_TRUE(composited_container_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(span_layer->SelfNeedsRepaint());
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
  auto* target_layer = To<LayoutBoxModelObject>(target)->Layer();
  auto* span = GetLayoutObjectByElementId("span");
  auto* span_layer = To<LayoutBoxModelObject>(span)->Layer();
  auto* text = span->SlowFirstChild();

  EXPECT_TRUE(span->IsPaintInvalidationContainer());
  EXPECT_TRUE(span->IsStackingContext());
  EXPECT_EQ(span, &EnclosingCompositedContainer(*target));
  EXPECT_EQ(target_layer, target->PaintingLayer());

  ValidateDisplayItemClient(target);
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    NGInlineCursor fragments;
    for (fragments.MoveTo(*span); fragments;
         fragments.MoveToNextForSameLayoutObject())
      ValidateDisplayItemClient(fragments.Current().GetDisplayItemClient());
  } else {
    ValidateDisplayItemClient(span);
    ValidateDisplayItemClient(text);
  }

  // Traversing from span should reach target.
  EXPECT_FALSE(span_layer->SelfNeedsRepaint());
  ObjectPaintInvalidator(*span)
      .InvalidatePaintIncludingNonCompositingDescendants();
  EXPECT_TRUE(span_layer->SelfNeedsRepaint());
}

TEST_F(ObjectPaintInvalidatorTest, Selection) {
  SetBodyInnerHTML("<img id='target' style='width: 100px; height: 100px'>");
  auto* target = GetLayoutObjectByElementId("target");

  // Add selection.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  GetDocument().GetFrame()->Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  const auto* graphics_layer = GetLayoutView().Layer()->GraphicsLayerBacking();
  const auto* invalidations =
      &graphics_layer->GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(1u, invalidations->size());
  EXPECT_EQ(IntRect(8, 8, 100, 100), (*invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kSelection, (*invalidations)[0].reason);
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Simulate a change without full invalidation or selection change.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->SetShouldCheckForPaintInvalidation();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(graphics_layer->GetRasterInvalidationTracking()
                  ->Invalidations()
                  .IsEmpty());
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Remove selection.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  GetDocument().GetFrame()->Selection().Clear();
  UpdateAllLifecyclePhasesForTest();
  invalidations =
      &graphics_layer->GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(1u, invalidations->size());
  EXPECT_EQ(IntRect(8, 8, 100, 100), (*invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kSelection, (*invalidations)[0].reason);
  GetDocument().View()->SetTracksRasterInvalidations(false);
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

TEST_F(ObjectPaintInvalidatorTest, VisibilityHidden) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        visibility: hidden;
        width: 100px;
        height: 100px;
        background: blue;
      }
    </style>
    <div id="target"></div>
  )HTML");

  auto* target_element = GetDocument().getElementById("target");
  const auto* target = target_element->GetLayoutObject();
  ValidateDisplayItemClient(target);
  EXPECT_TRUE(IsValidDisplayItemClient(target));

  target_element->setAttribute(html_names::kStyleAttr, "width: 200px");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(IsValidDisplayItemClient(target));
  UpdateAllLifecyclePhasesForTest();

  target_element->setAttribute(html_names::kStyleAttr,
                               "width: 200px; visibility: visible");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(IsValidDisplayItemClient(target));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsValidDisplayItemClient(target));

  target_element->setAttribute(html_names::kStyleAttr,
                               "width: 200px; visibility: hidden");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(IsValidDisplayItemClient(target));
  UpdateAllLifecyclePhasesForTest();
  // |target| is not validated because it didn't paint anything.
  EXPECT_FALSE(IsValidDisplayItemClient(target));
}

}  // namespace blink
