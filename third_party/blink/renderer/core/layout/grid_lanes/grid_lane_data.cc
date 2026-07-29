// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lane_data.h"

#include "base/check_op.h"

namespace blink {

namespace {

void AdjustItemPlacementOffset(LayoutUnit offset_adjustment,
                               bool is_block_direction,
                               GridLanesItemData& item_data) {
  if (item_data.is_item_start) {
    auto& item_offset =
        item_data.grid_lanes_placement_data->placement_data.offset;
    if (is_block_direction) {
      item_offset.block_offset += offset_adjustment;
    } else {
      item_offset.inline_offset += offset_adjustment;
    }
  }

  for (GridLanesItemData* packed_item : item_data.items_packed_above) {
    AdjustItemPlacementOffset(offset_adjustment, is_block_direction,
                              *packed_item);
  }
}

}  // namespace

void AddItemToGridLanesData(
    GridItemData& grid_lanes_item,
    GridLanesItemPlacementData* grid_lanes_placement_data,
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
        &grid_lanes_item, grid_lanes_placement_data,
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

GridLanesItemPlacementData* FindGridLanesItemPlacementData(
    const GridItemData& item,
    wtf_size_t item_index,
    GridTrackSizingDirection grid_axis_direction,
    const GridLanesDataVector* grid_lanes) {
  if (!grid_lanes) {
    return nullptr;
  }

  const wtf_size_t start_lane = item.StartLine(grid_axis_direction);
  CHECK_LT(start_lane, grid_lanes->size());

  const GridLaneData* lane_data = grid_lanes->at(start_lane);
  CHECK(lane_data);
  CHECK_LT(item_index, lane_data->item_data.size());

  GridLanesItemData* item_data = lane_data->item_data[item_index];
  if (item_data->item == &item) {
    return item_data->grid_lanes_placement_data;
  }

  // If the indexed item does not match, `item` was densely packed above this
  // spanner. Search for it among the spanner's packed items.
  for (GridLanesItemData* packed_item : item_data->items_packed_above) {
    if (packed_item->item == &item) {
      return packed_item->grid_lanes_placement_data;
    }
  }
  NOTREACHED();
}

void AdjustGridLanesItemPlacementOffsets(LayoutUnit offset_adjustment,
                                         bool is_block_direction,
                                         GridLanesDataVector& grid_lanes) {
  if (!offset_adjustment) {
    return;
  }

  for (GridLaneData* lane_data : grid_lanes) {
    if (!lane_data) {
      continue;
    }
    for (GridLanesItemData* item_data : lane_data->item_data) {
      AdjustItemPlacementOffset(offset_adjustment, is_block_direction,
                                *item_data);
    }
  }
}

void ReverseGridLanesItemOrder(GridLanesDataVector& grid_lanes) {
  for (GridLaneData* lane_data : grid_lanes) {
    if (!lane_data) {
      continue;
    }
    lane_data->item_data.Reverse();
    for (GridLanesItemData* item_data : lane_data->item_data) {
      item_data->items_packed_above.Reverse();
    }
  }
}

}  // namespace blink
