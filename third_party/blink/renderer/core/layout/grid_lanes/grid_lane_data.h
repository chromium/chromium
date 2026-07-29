// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANE_DATA_H_

#include "third_party/blink/renderer/core/layout/grid/grid_break_token_data.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// Placement data shared by every lane entry for a single grid-lanes item.
struct GridLanesItemPlacementData
    : public GarbageCollected<GridLanesItemPlacementData> {
  explicit GridLanesItemPlacementData(
      const GridItemPlacementData& placement_data)
      : placement_data(placement_data) {}

  void Trace(Visitor*) const {}

  GridItemPlacementData placement_data;

  // Space available for alignment in the stacking axis. Fragmentation may
  // increase this if the track opening expands.
  LayoutUnit available_stacking_axis_alignment_space;
};

// Item and placement data for a single grid lanes item. If the item is a
// spanner and dense packing is enabled, this will also store any items that
// were packed above it. Entries for the same spanner share
// `grid_lanes_placement_data`, while `is_item_start` and `items_packed_above`
// remain lane-specific.
struct GridLanesItemData : public GarbageCollected<GridLanesItemData> {
  GridLanesItemData(GridItemData* item,
                    GridLanesItemPlacementData* grid_lanes_placement_data,
                    bool is_item_start = true)
      : item(item),
        grid_lanes_placement_data(grid_lanes_placement_data),
        is_item_start(is_item_start) {}

  const GridItemPlacementData& PlacementData() const {
    return grid_lanes_placement_data->placement_data;
  }

  LayoutUnit AvailableStackingAxisAlignmentSpace() const {
    return grid_lanes_placement_data->available_stacking_axis_alignment_space;
  }

  void AddPackedItem(GridLanesItemData* packed_item) {
    items_packed_above.push_back(std::move(packed_item));
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(item);
    visitor->Trace(grid_lanes_placement_data);
    visitor->Trace(items_packed_above);
  }

  Member<GridItemData> item;
  Member<GridLanesItemPlacementData> grid_lanes_placement_data;

  // Whether this entry represents the start of a grid item. This will always be
  // true for items that span one track, but for spanners, it will only be true
  // for the first track it occupies.
  bool is_item_start = true;

  // Only set if this item is a spanner with items densely packed above it.
  HeapVector<Member<GridLanesItemData>> items_packed_above;
};

// Stores items placed in a single grid lane.
struct GridLaneData : public GarbageCollected<GridLaneData> {
  void AddItem(GridLanesItemData* item) {
    item_data.push_back(std::move(item));
  }

  void Trace(Visitor* visitor) const { visitor->Trace(item_data); }

  HeapVector<Member<GridLanesItemData>> item_data;
};

using GridLanesDataVector = HeapVector<Member<GridLaneData>, 1>;

// Adds an item entry to every lane occupied by its span.
//
// For a densely packed item, `spanner_indices_below_opening` has one index per
// occupied lane. Each index identifies the spanner in that lane's
// `GridLaneData::item_data` under which the new entry should be nested.
// `kNotFound` indicates that the lane has no spanner below the opening, so the
// new entry is added directly to the lane. The vector is empty for an item that
// was not densely packed.
void AddItemToGridLanesData(
    GridItemData& grid_lanes_item,
    GridLanesItemPlacementData* grid_lanes_placement_data,
    const Vector<wtf_size_t>& spanner_indices_below_opening,
    GridTrackSizingDirection grid_axis_direction,
    GridLanesDataVector& out_grid_lanes);

// Returns the placement data for `item`. `item_index` identifies either the
// item itself or the spanner it was densely packed above in the item's start
// lane. Returns null when `grid_lanes` is not provided.
GridLanesItemPlacementData* FindGridLanesItemPlacementData(
    const GridItemData& item,
    wtf_size_t item_index,
    GridTrackSizingDirection grid_axis_direction,
    const GridLanesDataVector* grid_lanes);

// Applies an offset adjustment once to each shared item placement record.
void AdjustGridLanesItemPlacementOffsets(LayoutUnit offset_adjustment,
                                         bool is_block_direction,
                                         GridLanesDataVector& grid_lanes);

// Reverses the direct and packed item order within each lane.
void ReverseGridLanesItemOrder(GridLanesDataVector& grid_lanes);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANE_DATA_H_
