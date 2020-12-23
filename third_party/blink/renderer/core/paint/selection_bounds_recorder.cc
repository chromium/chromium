// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/selection_bounds_recorder.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/api/selection_state.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

SelectionBoundsRecorder::SelectionBoundsRecorder(
    SelectionState state,
    PhysicalRect selection_rect,
    PaintController& paint_controller)
    : state_(state),
      selection_rect_(selection_rect),
      paint_controller_(paint_controller) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
}

SelectionBoundsRecorder::~SelectionBoundsRecorder() {
  // TODO(crbug.com/1065049) Handle RTL (i.e. IsTextDirectionRTL) to adjust
  // the type and edges appropriately (i.e. the right edge of the selection rect
  // should be used for start's edges).
  base::Optional<PaintedSelectionBound> start;
  base::Optional<PaintedSelectionBound> end;
  auto selection_rect = PixelSnappedIntRect(selection_rect_);
  if (state_ == SelectionState::kStart ||
      state_ == SelectionState::kStartAndEnd) {
    start.emplace();
    start->type = gfx::SelectionBound::Type::LEFT;
    start->edge_start = selection_rect.MinXMinYCorner();
    start->edge_end = selection_rect.MinXMaxYCorner();
    // TODO(crbug.com/1065049) Handle the case where selection within input
    // text is clipped out.
    start->hidden = false;
  }

  if (state_ == SelectionState::kStartAndEnd ||
      state_ == SelectionState::kEnd) {
    end.emplace();
    end->type = gfx::SelectionBound::Type::RIGHT;
    end->edge_start = selection_rect.MaxXMinYCorner();
    end->edge_end = selection_rect.MaxXMaxYCorner();
    end->hidden = false;
  }

  paint_controller_.RecordSelection(start, end);
}

bool SelectionBoundsRecorder::ShouldRecordSelection(
    const FrameSelection& frame_selection,
    SelectionState state) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return false;

  if (!frame_selection.IsHandleVisible() || frame_selection.IsHidden())
    return false;

  if (state == SelectionState::kInside || state == SelectionState::kNone)
    return false;

  return true;
}

}  // namespace blink
