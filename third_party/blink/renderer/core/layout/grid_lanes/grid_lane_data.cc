// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lane_data.h"

#include "base/check_op.h"

namespace blink {

void AddItemToGridLanesData(GridItemData& grid_lanes_item,
                            const GridItemPlacementData& placement_data,
                            GridTrackSizingDirection grid_axis_direction,
                            GridLanesDataVector& out_grid_lanes) {
  const GridSpan& span = grid_lanes_item.Span(grid_axis_direction);
  CHECK_LE(span.EndLine(), out_grid_lanes.size());

  for (wtf_size_t track_index = span.StartLine(); track_index < span.EndLine();
       ++track_index) {
    auto* item_data = MakeGarbageCollected<GridLanesItemData>(
        &grid_lanes_item, placement_data,
        /*is_item_start=*/track_index == span.StartLine());
    auto& lane_data = out_grid_lanes.at(track_index);
    if (!lane_data) {
      lane_data = MakeGarbageCollected<GridLaneData>();
    }
    lane_data->AddItem(item_data);
  }
}

}  // namespace blink
