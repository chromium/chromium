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

// Note: If the layout object has a scrolling contents layer, the selection
// will be relative to that.
static GraphicsLayer* GetGraphicsLayerBacking(
    const LayoutObject& layout_object) {
  const LayoutBoxModelObject& paint_invalidation_container =
      layout_object.ContainerForPaintInvalidation();
  DCHECK(paint_invalidation_container.Layer());
  if (paint_invalidation_container.Layer()->GetCompositingState() ==
      kNotComposited)
    return nullptr;
  return paint_invalidation_container.Layer()->GraphicsLayerBacking(
      &layout_object);
}

// Convert a local point into the coordinate system of backing coordinates.
static gfx::Point LocalToInvalidationBackingPoint(
    const LayoutPoint& local_point,
    const LayoutObject& layout_object) {
  const LayoutBoxModelObject& paint_invalidation_container =
      layout_object.ContainerForPaintInvalidation();
  DCHECK(paint_invalidation_container.Layer());

  FloatPoint container_point = layout_object.LocalToAncestorPoint(
      FloatPoint(local_point), &paint_invalidation_container,
      kTraverseDocumentBoundaries);

  // A layoutObject can have no invalidation backing if it is from a detached
  // frame, or when forced compositing is disabled.
  if (paint_invalidation_container.Layer()->GetCompositingState() ==
      kNotComposited)
    return RoundedIntPoint(container_point);

  PaintLayer::MapPointInPaintInvalidationContainerToBacking(
      paint_invalidation_container, container_point);

  if (GraphicsLayer* graphics_layer = GetGraphicsLayerBacking(layout_object))
    container_point.Move(-graphics_layer->OffsetFromLayoutObject());

  // Ensure the coordinates are in the scrolling contents space, if the object
  // is a scroller.
  if (paint_invalidation_container.UsesCompositedScrolling()) {
    container_point.Move(paint_invalidation_container.Layer()
                             ->GetScrollableArea()
                             ->GetScrollOffset());
  }

  return RoundedIntPoint(container_point);
}

std::pair<LayoutPoint, LayoutPoint> static GetLocalSelectionStartpoints(
    const LocalCaretRect& local_caret_rect) {
  const LayoutRect rect = local_caret_rect.rect;
  if (local_caret_rect.layout_object->Style()->IsHorizontalWritingMode())
    return {rect.MinXMinYCorner(), rect.MinXMaxYCorner()};

  // When text is vertical, it looks better for the start handle baseline to
  // be at the starting edge, to enclose the selection fully between the
  // handles.
  return {rect.MaxXMinYCorner(), rect.MinXMinYCorner()};
}

std::pair<LayoutPoint, LayoutPoint> static GetLocalSelectionEndpoints(
    const LocalCaretRect& local_caret_rect) {
  const LayoutRect rect = local_caret_rect.rect;
  if (local_caret_rect.layout_object->Style()->IsHorizontalWritingMode())
    return {rect.MinXMinYCorner(), rect.MinXMaxYCorner()};

  return {rect.MinXMinYCorner(), rect.MaxXMinYCorner()};
}

static LayoutPoint GetSamplePointForVisibility(
    const LayoutPoint& edge_top_in_layer,
    const LayoutPoint& edge_bottom_in_layer,
    float zoom_factor) {
  FloatSize diff(edge_top_in_layer - edge_bottom_in_layer);
  // Adjust by ~1px to avoid integer snapping error. This logic is the same
  // as that in ComputeViewportSelectionBound in cc.
  diff.Scale(zoom_factor / diff.DiagonalLength());
  LayoutPoint sample_point = edge_bottom_in_layer;
  sample_point.Move(LayoutSize(diff));
  return sample_point;
}

// Returns whether this position is not visible on the screen (because
// clipped out).
static bool IsVisible(const LayoutObject& rect_layout_object,
                      const LayoutPoint& edge_top_in_layer,
                      const LayoutPoint& edge_bottom_in_layer) {
  Node* const node = rect_layout_object.GetNode();
  if (!node)
    return true;
  TextControlElement* text_control = EnclosingTextControl(node);
  if (!text_control)
    return true;
  if (!IsHTMLInputElement(text_control))
    return true;

  LayoutObject* layout_object = text_control->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return true;

  const LayoutPoint sample_point =
      GetSamplePointForVisibility(edge_top_in_layer, edge_bottom_in_layer,
                                  rect_layout_object.View()->ZoomFactor());

  LayoutBox* const text_control_object = ToLayoutBox(layout_object);
  const LayoutPoint position_in_input(rect_layout_object.LocalToAncestorPoint(
      FloatPoint(sample_point), text_control_object,
      kTraverseDocumentBoundaries));
  return text_control_object->BorderBoxRect().Contains(position_in_input);
}

static base::Optional<cc::LayerSelectionBound> ComputeSelectionBound(
    const LayoutObject& layout_object,
    const LayoutPoint& edge_top_in_layer,
    const LayoutPoint& edge_bottom_in_layer) {
  cc::LayerSelectionBound bound;

  bound.edge_top =
      LocalToInvalidationBackingPoint(edge_top_in_layer, layout_object);
  bound.edge_bottom =
      LocalToInvalidationBackingPoint(edge_bottom_in_layer, layout_object);
  GraphicsLayer* const layer = GetGraphicsLayerBacking(layout_object);
  if (!layer)
    return base::nullopt;
  bound.layer_id = layer->CcLayer()->id();
  bound.hidden =
      !IsVisible(layout_object, edge_top_in_layer, edge_bottom_in_layer);
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

static base::Optional<cc::LayerSelectionBound>
StartPositionInGraphicsLayerBacking(const SelectionInDOMTree& selection) {
  const PositionWithAffinity position(selection.ComputeStartPosition(),
                                      selection.Affinity());
  const LocalCaretRect& local_caret_rect = LocalCaretRectOfPosition(position);
  const LayoutObject* const layout_object = local_caret_rect.layout_object;
  if (!layout_object)
    return base::nullopt;

  LayoutPoint edge_top_in_layer, edge_bottom_in_layer;
  std::tie(edge_top_in_layer, edge_bottom_in_layer) =
      GetLocalSelectionStartpoints(local_caret_rect);
  const auto& maybe = ComputeSelectionBound(*layout_object, edge_top_in_layer,
                                            edge_bottom_in_layer);
  if (!maybe.has_value())
    return base::nullopt;
  cc::LayerSelectionBound bound = maybe.value();
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

  LayoutPoint edge_top_in_layer, edge_bottom_in_layer;
  std::tie(edge_top_in_layer, edge_bottom_in_layer) =
      GetLocalSelectionEndpoints(local_caret_rect);
  const auto& maybe = ComputeSelectionBound(*layout_object, edge_top_in_layer,
                                            edge_bottom_in_layer);
  if (!maybe.has_value())
    return base::nullopt;
  cc::LayerSelectionBound bound = maybe.value();
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
