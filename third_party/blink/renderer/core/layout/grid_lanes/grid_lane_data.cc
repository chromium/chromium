// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lane_data.h"

#include "base/check_op.h"

namespace blink {

void AddItemToGridLanesData(
    GridItemData& grid_lanes_item,
    const GridItemPlacementData& placement_data,
    const Vector<wtf_size_t>& spanner_indices_below_opening,
    GridTrackSizingDirection grid_axis_direction,
    GridLanesDataVector& out_grid_lanes) {
  const GridSpan& span = grid_lanes_item.Span(grid_axis_direction);
  CHECK_LE(span.EndLine(), out_grid_lanes.size());
  CHECK(spanner_indices_below_opening.empty() ||
        spanner_indices_below_opening.size() == span.SpanSize());

  for (wtf_size_t track_index = span.StartLine(); track_index < span.EndLine();
       ++track_index) {
    auto* item_data = MakeGarbageCollected<GridLanesItemData>(
        &grid_lanes_item, placement_data,
        /*is_item_start=*/track_index == span.StartLine());

    auto& lane_data = out_grid_lanes.at(track_index);
    if (!lane_data) {
      lane_data = MakeGarbageCollected<GridLaneData>();
    }

    // An empty vector means the item used its normal placement, so its entry is
    // appended directly to each lane.
    if (spanner_indices_below_opening.empty()) {
      lane_data->AddItem(item_data);
    } else {
      // Dense placement supplies the index of the spanner below the selected
      // opening for each lane. An item may be densely packed above a spanner in
      // some tracks while spanning a track with no spanner below. `kNotFound`
      // indicates that the item should be added normally in that track.
      const wtf_size_t span_index = track_index - span.StartLine();
      const wtf_size_t spanner_below_index =
          spanner_indices_below_opening[span_index];
      if (spanner_below_index == kNotFound) {
        lane_data->AddItem(item_data);
      } else {
        CHECK_LT(spanner_below_index, lane_data->item_data.size());
        lane_data->item_data[spanner_below_index]->AddPackedItem(item_data);
      }
    }
  }
}

}  // namespace blink
