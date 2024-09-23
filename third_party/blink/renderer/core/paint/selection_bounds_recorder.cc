// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/selection_bounds_recorder.h"

#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/selection_state.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

namespace {

// This represents a directional edge of a rect, starting at one corner and
// ending on another. Note that the 'left' and 'right' edges only have one
// variant because the edge always ends on the bottom. However in vertical
// writing modes, the edge end should follow the block direction, which can
// be flipped.
enum class RectEdge {
  kTopLeftToBottomLeft,
  kTopRightToBottomRight,
  kTopLeftToTopRight,
  kBottomLeftToBottomRight,
  kTopRightToTopLeft,
  kBottomRightToBottomLeft,
};

struct BoundEdges {
  RectEdge start;
  RectEdge end;
  DISALLOW_NEW();
};

// Based on the given WritingMode and direction, return the pair of start and
// end edges that should be used to determe the PaintedSelectionBound start
// and end edges given a selection rectangle. For the simplest cases (i.e.
// LTR horizontal writing mode), the left edge is the start and the right edge
// would be the end. However, this flips for RTL, and vertical writing modes
// additionally complicated matters.
BoundEdges GetBoundEdges(WritingMode writing_mode, bool is_ltr) {
  if (IsHorizontalWritingMode(writing_mode)) {
    if (is_ltr)
      return {RectEdge::kTopLeftToBottomLeft, RectEdge::kTopRightToBottomRight};
    else
      return {RectEdge::kTopRightToBottomRight, RectEdge::kTopLeftToBottomLeft};
  } else if (IsFlippedBlocksWritingMode(writing_mode)) {
    if (is_ltr)
      return {RectEdge::kTopLeftToTopRight, RectEdge::kBottomRightToBottomLeft};
    else
      return {RectEdge::kBottomLeftToBottomRight, RectEdge::kTopRightToTopLeft};
  } else {
    if (is_ltr)
      return {RectEdge::kTopRightToTopLeft, RectEdge::kBottomLeftToBottomRight};
    else
      return {RectEdge::kBottomRightToBottomLeft, RectEdge::kTopLeftToTopRight};
  }
}

// Set the given bound's edge_start and edge_end, based on the provided
// selection rect and edge.
void SetBoundEdge(gfx::Rect selection_rect,
                  RectEdge edge,
                  PaintedSelectionBound& bound) {
  switch (edge) {
    case RectEdge::kTopLeftToBottomLeft:
      bound.edge_start = selection_rect.origin();
      bound.edge_end = selection_rect.bottom_left();
      return;
    case RectEdge::kTopRightToBottomRight:
      bound.edge_start = selection_rect.top_right();
      bound.edge_end = selection_rect.bottom_right();
      return;
    case RectEdge::kTopLeftToTopRight:
      bound.edge_start = selection_rect.origin();
      bound.edge_end = selection_rect.top_right();
      return;
    case RectEdge::kBottomLeftToBottomRight:
      bound.edge_start = selection_rect.bottom_left();
      bound.edge_end = selection_rect.bottom_right();
      return;
    case RectEdge::kTopRightToTopLeft:
      bound.edge_start = selection_rect.top_right();
      bound.edge_end = selection_rect.origin();
      return;
    case RectEdge::kBottomRightToBottomLeft:
      bound.edge_start = selection_rect.bottom_right();
      bound.edge_end = selection_rect.bottom_left();
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

PhysicalOffset GetSamplePointForVisibility(const PhysicalOffset& edge_start,
                                           const PhysicalOffset& edge_end,
                                           float zoom_factor) {
  gfx::Vector2dF diff(edge_start - edge_end);
  // Adjust by ~1px to avoid integer snapping error. This logic is the same
  // as that in ComputeViewportSelectionBound in cc.
  diff.Scale(zoom_factor / diff.Length());
  PhysicalOffset sample_point = edge_end;
  sample_point += PhysicalOffset::FromVector2dFRound(diff);
  return sample_point;
}

}  // namespace

SelectionBoundsRecorder::SelectionBoundsRecorder(
    SelectionState state,
    PhysicalRect selection_rect,
    PaintController& paint_controller,
    TextDirection text_direction,
    WritingMode writing_mode,
    const LayoutObject& layout_object)
    : state_(state),
      selection_rect_(selection_rect),
      paint_controller_(paint_controller),
      text_direction_(text_direction),
      writing_mode_(writing_mode),
      selection_layout_object_(layout_object) {}

SelectionBoundsRecorder::~SelectionBoundsRecorder() {
  paint_controller_.RecordAnySelectionWasPainted();

  if (state_ == SelectionState::kInside)
    return;

  std::optional<PaintedSelectionBound> start;
  std::optional<PaintedSelectionBound> end;
  gfx::Rect selection_rect = ToPixelSnappedRect(selection_rect_);
  const bool is_ltr = IsLtr(text_direction_);
  BoundEdges edges = GetBoundEdges(writing_mode_, is_ltr);
  if (state_ == SelectionState::kStart ||
      state_ == SelectionState::kStartAndEnd) {
    start.emplace();
    start->type = is_ltr ? gfx::SelectionBound::Type::LEFT
                         : gfx::SelectionBound::Type::RIGHT;
    SetBoundEdge(selection_rect, edges.start, *start);
    start->hidden =
        !IsVisible(selection_layout_object_, PhysicalOffset(start->edge_start),
                   PhysicalOffset(start->edge_end));
  }

  if (state_ == SelectionState::kStartAndEnd ||
      state_ == SelectionState::kEnd) {
    end.emplace();
    end->type = is_ltr ? gfx::SelectionBound::Type::RIGHT
                       : gfx::SelectionBound::Type::LEFT;
    SetBoundEdge(selection_rect, edges.end, *end);
    end->hidden =
        !IsVisible(selection_layout_object_, PhysicalOffset(end->edge_start),
                   PhysicalOffset(end->edge_end));
  }

  paint_controller_.RecordSelection(start, end, "");
}

bool SelectionBoundsRecorder::ShouldRecordSelection(
    const FrameSelection& frame_selection,
    SelectionState state) {
  if (!frame_selection.IsHandleVisible() || frame_selection.IsHidden())
    return false;

  // If the currently focused frame is not the one in which selection
  // lives, don't paint the selection bounds. Note this is subtly different
  // from whether the frame has focus (i.e. `FrameSelection::SelectionHasFocus`)
  // which is false if the hosting window is not focused.
  LocalFrame* local_frame = frame_selection.GetFrame();
  LocalFrame* focused_frame =
      local_frame->GetPage()->GetFocusController().FocusedFrame();
  if (local_frame != focused_frame)
    return false;

  if (state == SelectionState::kNone)
    return false;

  return true;
}

// Returns whether this position is not visible on the screen (because
// clipped out).
bool SelectionBoundsRecorder::IsVisible(const LayoutObject& rect_layout_object,
                                        const PhysicalOffset& edge_start,
                                        const PhysicalOffset& edge_end) {
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

  PhysicalOffset sample_point = GetSamplePointForVisibility(
      edge_start, edge_end, rect_layout_object.GetFrame()->LayoutZoomFactor());

  // Convert from paint coordinates to local layout coordinates.
  sample_point -= layout_object->FirstFragment().PaintOffset();

  auto* const text_control_object = To<LayoutBox>(layout_object);
  const PhysicalOffset position_in_input =
      rect_layout_object.LocalToAncestorPoint(sample_point, text_control_object,
                                              kTraverseDocumentBoundaries);
  return text_control_object->PhysicalBorderBoxRect().Contains(
      position_in_input);
}

}  // namespace blink
