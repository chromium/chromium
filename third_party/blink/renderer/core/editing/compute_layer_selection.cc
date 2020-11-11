/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/compute_layer_selection.h"

#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"

#include <unicode/ubidi.h>

namespace blink {

const PaintLayer* EnclosingCompositedContainer(
    const LayoutObject& layout_object) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  return layout_object.PaintingLayer()
      ->EnclosingLayerForPaintInvalidationCrossingFrameBoundaries();
}

// Convert a local point into the coordinate system of backing coordinates.
static gfx::Point LocalToInvalidationBackingPoint(
    const PhysicalOffset& local_point,
    const LayoutObject& layout_object,
    const GraphicsLayer& graphics_layer) {
  const PaintLayer* paint_invalidation_container =
      EnclosingCompositedContainer(layout_object);
  if (!paint_invalidation_container)
    return gfx::Point();

  PhysicalOffset container_point = layout_object.LocalToAncestorPoint(
      local_point, &paint_invalidation_container->GetLayoutObject(),
      kTraverseDocumentBoundaries);

  // A layoutObject can have no invalidation backing if it is from a detached
  // frame, or when forced compositing is disabled.
  if (paint_invalidation_container->GetCompositingState() == kNotComposited)
    return RoundedIntPoint(container_point);

  PaintLayer::MapPointInPaintInvalidationContainerToBacking(
      paint_invalidation_container->GetLayoutObject(), container_point);
  container_point -= PhysicalOffset(graphics_layer.OffsetFromLayoutObject());

  // Ensure the coordinates are in the scrolling contents space, if the object
  // is a scroller.
  if (paint_invalidation_container->GetLayoutObject()
          .UsesCompositedScrolling()) {
    container_point += PhysicalOffset::FromFloatSizeRound(
        paint_invalidation_container->GetScrollableArea()->GetScrollOffset());
  }

  return RoundedIntPoint(container_point);
}

std::pair<PhysicalOffset, PhysicalOffset> static GetLocalSelectionStartpoints(
    const LocalCaretRect& local_caret_rect) {
  const PhysicalRect rect = local_caret_rect.rect;
  if (local_caret_rect.layout_object->IsHorizontalWritingMode())
    return {rect.MinXMinYCorner(), rect.MinXMaxYCorner()};

  // When text is vertical, it looks better for the start handle baseline to
  // be at the starting edge, to enclose the selection fully between the
  // handles.
  if (local_caret_rect.layout_object->HasFlippedBlocksWritingMode())
    return {rect.MinXMinYCorner(), rect.MaxXMinYCorner()};
  return {rect.MaxXMinYCorner(), rect.MinXMinYCorner()};
}

std::pair<PhysicalOffset, PhysicalOffset> static GetLocalSelectionEndpoints(
    const LocalCaretRect& local_caret_rect) {
  const PhysicalRect rect = local_caret_rect.rect;
  if (local_caret_rect.layout_object->IsHorizontalWritingMode())
    return {rect.MinXMinYCorner(), rect.MinXMaxYCorner()};

  if (local_caret_rect.layout_object->HasFlippedBlocksWritingMode())
    return {rect.MaxXMinYCorner(), rect.MinXMinYCorner()};
  return {rect.MinXMinYCorner(), rect.MaxXMinYCorner()};
}

static PhysicalOffset GetSamplePointForVisibility(
    const PhysicalOffset& edge_start_in_layer,
    const PhysicalOffset& edge_end_in_layer,
    float zoom_factor) {
  FloatSize diff(edge_start_in_layer - edge_end_in_layer);
  // Adjust by ~1px to avoid integer snapping error. This logic is the same
  // as that in ComputeViewportSelectionBound in cc.
  diff.Scale(zoom_factor / diff.DiagonalLength());
  PhysicalOffset sample_point = edge_end_in_layer;
  sample_point += PhysicalOffset::FromFloatSizeRound(diff);
  return sample_point;
}

// Returns whether this position is not visible on the screen (because
// clipped out).
static bool IsVisible(const LayoutObject& rect_layout_object,
                      const PhysicalOffset& edge_start_in_layer,
                      const PhysicalOffset& edge_end_in_layer) {
  Node* const node = rect_layout_object.GetNode();
  if (!node)
    return true;
  TextControlElement* text_control = EnclosingTextControl(node);
  if (!text_control)
    return true;
  if (!IsA<HTMLInputElement>(text_control))
    return true;

  LayoutObject* layout_object = text_control->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return true;

  const PhysicalOffset sample_point =
      GetSamplePointForVisibility(edge_start_in_layer, edge_end_in_layer,
                                  rect_layout_object.View()->ZoomFactor());

  auto* const text_control_object = To<LayoutBox>(layout_object);
  const PhysicalOffset position_in_input =
      rect_layout_object.LocalToAncestorPoint(sample_point, text_control_object,
                                              kTraverseDocumentBoundaries);
  return text_control_object->PhysicalBorderBoxRect().Contains(
      position_in_input);
}

static cc::LayerSelectionBound ComputeSelectionBound(
    const LayoutObject& layout_object,
    const GraphicsLayer& graphics_layer,
    const PhysicalOffset& edge_start_in_layer,
    const PhysicalOffset& edge_end_in_layer) {
  cc::LayerSelectionBound bound;

  bound.edge_start = LocalToInvalidationBackingPoint(
      edge_start_in_layer, layout_object, graphics_layer);
  bound.edge_end = LocalToInvalidationBackingPoint(
      edge_end_in_layer, layout_object, graphics_layer);
  bound.layer_id = graphics_layer.CcLayer().id();
  bound.hidden =
      !IsVisible(layout_object, edge_start_in_layer, edge_end_in_layer);
  return bound;
}

// TODO(yoichio): Fix following weird implementation:
// 1. IsTextDirectionRTL is computed from original Node and
// LayoutObject from LocalCaretRectOfPosition.
// 2. Current code can make both selection handles "LEFT".
static inline bool IsTextDirectionRTL(const Node& node,
                                      const LayoutObject& layout_object) {
  return layout_object.HasFlippedBlocksWritingMode() ||
         PrimaryDirectionOf(node) == TextDirection::kRtl;
}

static GraphicsLayer* GetGraphicsLayerFor(const LayoutObject& layout_object) {
  const PaintLayer* paint_invalidation_container =
      EnclosingCompositedContainer(layout_object);
  if (!paint_invalidation_container)
    return nullptr;
  if (paint_invalidation_container->GetCompositingState() == kNotComposited)
    return nullptr;
  return paint_invalidation_container->GraphicsLayerBacking(&layout_object);
}

static base::Optional<cc::LayerSelectionBound>
StartPositionInGraphicsLayerBacking(const SelectionInDOMTree& selection) {
  const PositionWithAffinity position(selection.ComputeStartPosition(),
                                      selection.Affinity());
  const LocalCaretRect& local_caret_rect = LocalCaretRectOfPosition(position);
  const LayoutObject* const layout_object = local_caret_rect.layout_object;
  if (!layout_object)
    return base::nullopt;
  GraphicsLayer* graphics_layer = GetGraphicsLayerFor(*layout_object);
  if (!graphics_layer)
    return base::nullopt;

  PhysicalOffset edge_start_in_layer, edge_end_in_layer;
  std::tie(edge_start_in_layer, edge_end_in_layer) =
      GetLocalSelectionStartpoints(local_caret_rect);
  cc::LayerSelectionBound bound = ComputeSelectionBound(
      *layout_object, *graphics_layer, edge_start_in_layer, edge_end_in_layer);
  if (selection.IsRange()) {
    bound.type = IsTextDirectionRTL(*position.AnchorNode(), *layout_object)
                     ? gfx::SelectionBound::Type::RIGHT
                     : gfx::SelectionBound::Type::LEFT;
  } else {
    bound.type = gfx::SelectionBound::Type::CENTER;
  }
  return bound;
}

static base::Optional<cc::LayerSelectionBound>
EndPositionInGraphicsLayerBacking(const SelectionInDOMTree& selection) {
  const PositionWithAffinity position(selection.ComputeEndPosition(),
                                      selection.Affinity());
  const LocalCaretRect& local_caret_rect = LocalCaretRectOfPosition(position);
  const LayoutObject* const layout_object = local_caret_rect.layout_object;
  if (!layout_object)
    return base::nullopt;
  GraphicsLayer* graphics_layer = GetGraphicsLayerFor(*layout_object);
  if (!graphics_layer)
    return base::nullopt;

  PhysicalOffset edge_start_in_layer, edge_end_in_layer;
  std::tie(edge_start_in_layer, edge_end_in_layer) =
      GetLocalSelectionEndpoints(local_caret_rect);
  cc::LayerSelectionBound bound = ComputeSelectionBound(
      *layout_object, *graphics_layer, edge_start_in_layer, edge_end_in_layer);
  if (selection.IsRange()) {
    bound.type = IsTextDirectionRTL(*position.AnchorNode(), *layout_object)
                     ? gfx::SelectionBound::Type::LEFT
                     : gfx::SelectionBound::Type::RIGHT;
  } else {
    bound.type = gfx::SelectionBound::Type::CENTER;
  }
  return bound;
}

cc::LayerSelection ComputeLayerSelection(
    const FrameSelection& frame_selection) {
  // TODO(https://crbug.com/1065049) - Implement layer selection for
  // CompositeAfterPaint
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return {};

  if (!frame_selection.IsHandleVisible() || frame_selection.IsHidden())
    return {};

  // TODO(yoichio): Compute SelectionInDOMTree w/o VS canonicalization.
  // crbug.com/789870 for detail.
  const SelectionInDOMTree& selection =
      frame_selection.ComputeVisibleSelectionInDOMTree().AsSelection();
  if (selection.IsNone())
    return {};
  // Non-editable caret selections lack any kind of UI affordance, and
  // needn't be tracked by the client.
  if (selection.IsCaret() &&
      !IsEditablePosition(selection.ComputeStartPosition()))
    return {};

  const auto& maybe_start_bound =
      StartPositionInGraphicsLayerBacking(selection);
  if (!maybe_start_bound.has_value())
    return {};
  const auto& maybe_end_bound = EndPositionInGraphicsLayerBacking(selection);
  if (!maybe_end_bound.has_value())
    return {};
  return {maybe_start_bound.value(), maybe_end_bound.value()};
}

}  // namespace blink
