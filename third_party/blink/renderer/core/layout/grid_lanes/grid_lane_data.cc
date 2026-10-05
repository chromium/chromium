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

  for (GridLanesItemData* packed_item : item_data.items_densely_packed_above) {
    AdjustItemPlacementOffset(offset_adjustment, is_block_direction,
                              *packed_item);
  }
}

void ReverseDenselyPackedItemOrder(GridLanesItemData& item_data) {
  item_data.items_densely_packed_above.Reverse();
  for (GridLanesItemData* packed_item : item_data.items_densely_packed_above) {
    ReverseDenselyPackedItemOrder(*packed_item);
  }
}

}  // namespace

GridLanesItemData* GridLanesItemDataFromPath(const GridLaneData& lane_data,
                                             const ItemIndexPath& index_path) {
  const HeapVector<Member<GridLanesItemData>>* items = &lane_data.item_data;
  GridLanesItemData* item_data = nullptr;
  for (wtf_size_t index : index_path) {
    CHECK_LT(index, items->size());
    item_data = items->at(index);
    items = &item_data->items_densely_packed_above;
  }
  return item_data;
}

void AddItemToGridLanesData(
    GridItemData& grid_lanes_item,
    GridLanesItemPlacementData* grid_lanes_placement_data,
    const Vector<ItemIndexPath>& parent_item_index_path_per_lane,
    GridTrackSizingDirection grid_axis_direction,
    GridLanesDataVector& out_grid_lanes) {
  const GridSpan& span = grid_lanes_item.Span(grid_axis_direction);
  CHECK_LE(span.EndLine(), out_grid_lanes.size());
  CHECK(parent_item_index_path_per_lane.empty() ||
        parent_item_index_path_per_lane.size() == span.SpanSize());

  for (wtf_size_t track_index = span.StartLine(); track_index < span.EndLine();
       ++track_index) {
    auto* item_data = MakeGarbageCollected<GridLanesItemData>(
        &grid_lanes_item, grid_lanes_placement_data,
        /*is_item_start=*/track_index == span.StartLine());

    auto& lane_data = out_grid_lanes.at(track_index);
    if (!lane_data) {
      lane_data = MakeGarbageCollected<GridLaneData>();
    }

    // Spanner layout happens in the first lane it occupies. If a lane only has
    // non-start entries for a spanner, it effectively has no unfinished items.
    if (item_data->is_item_start) {
      lane_data->has_unfinished_items = true;
    }

    GridLanesItemData* spanner_below = nullptr;
    if (!parent_item_index_path_per_lane.empty()) {
      spanner_below = GridLanesItemDataFromPath(
          *lane_data,
          parent_item_index_path_per_lane[track_index - span.StartLine()]);
    }

    if (spanner_below) {
      spanner_below->AddDenselyPackedItem(item_data);
    } else {
      lane_data->AddItem(item_data);
    }
  }
}

GridLanesItemPlacementData* FindGridLanesItemPlacementData(
    const GridItemData& item,
    GridTrackSizingDirection grid_axis_direction,
    const ItemIndexPath& item_index_path,
    const GridLanesDataVector* grid_lanes) {
  if (!grid_lanes) {
    return nullptr;
  }

  const wtf_size_t start_lane = item.StartLine(grid_axis_direction);
  CHECK_LT(start_lane, grid_lanes->size());

  const GridLaneData* lane_data = grid_lanes->at(start_lane);
  CHECK(lane_data);

  const GridLanesItemData* item_data =
      GridLanesItemDataFromPath(*lane_data, item_index_path);
  CHECK(item_data);
  return item_data->grid_lanes_placement_data;
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
      ReverseDenselyPackedItemOrder(*item_data);
    }
  }
}

}  // namespace blink
