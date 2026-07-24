// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_gap_accumulator.h"

#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"

namespace blink {

GridLanesGapAccumulator::GridLanesGapAccumulator()
    : gap_geometry_(MakeGarbageCollected<GapGeometry>(
          GapGeometry::ContainerType::kGridLanes)) {}

void GridLanesGapAccumulator::BuildMainGaps(
    const GridLayoutTrackCollection& grid_axis_tracks) {
  const GridTrackGapData gap_data = BuildGridTrackGapData(
      grid_axis_tracks, GridTrackGapType::kMain, *gap_geometry_);
  const LayoutUnit gutter_size = grid_axis_tracks.GutterSize();

  const GridTrackSizingDirection grid_axis_direction =
      grid_axis_tracks.Direction();
  gap_geometry_->SetMainDirection(grid_axis_direction);
  if (grid_axis_direction == kForColumns) {
    gap_geometry_->SetInlineGapSize(gutter_size);
    gap_geometry_->SetContentInlineOffsets(gap_data.content_start,
                                           gap_data.content_end);
  } else {
    gap_geometry_->SetBlockGapSize(gutter_size);
    gap_geometry_->SetContentBlockOffsets(gap_data.content_start,
                                          gap_data.content_end);
  }
}

const GapGeometry* GridLanesGapAccumulator::FinalizeGapGeometry(
    LayoutUnit stacking_content_start,
    LayoutUnit stacking_content_end) {
  if (gap_geometry_->GetMainDirection() == kForColumns) {
    gap_geometry_->SetContentBlockOffsets(stacking_content_start,
                                          stacking_content_end);
  } else {
    gap_geometry_->SetContentInlineOffsets(stacking_content_start,
                                           stacking_content_end);
  }
  return gap_geometry_->MainGapCount() ? gap_geometry_ : nullptr;
}

}  // namespace blink
