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

using ItemIndexPath = Vector<wtf_size_t>;

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

  // Start of the item's opening in forward stacking order. Self-alignment and
  // fill-reverse may move the item without changing this position.
  LayoutUnit forward_stacking_start;

  // Index of the item's fragment in the container builder during normal layout.
  // Unset during fragmentation collection.
  wtf_size_t builder_child_index = kNotFound;
};

// Item and placement data for a single grid lanes item. With dense packing,
// this also stores any items packed directly above it. Entries for the same
// spanner share `grid_lanes_placement_data`, while `is_item_start` and
// `items_densely_packed_above` remain lane-specific.
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

  LayoutUnit ForwardStackingStart() const {
    return grid_lanes_placement_data->forward_stacking_start;
  }

  void AddDenselyPackedItem(GridLanesItemData* packed_item) {
    items_densely_packed_above.push_back(std::move(packed_item));
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(item);
    visitor->Trace(grid_lanes_placement_data);
    visitor->Trace(items_densely_packed_above);
  }

  Member<GridItemData> item;
  Member<GridLanesItemPlacementData> grid_lanes_placement_data;

  // Whether this entry represents the start of a grid item. This will always be
  // true for items that span one track, but for spanners, it will only be true
  // for the first track it occupies.
  bool is_item_start = true;

  // Items densely packed directly above this item in this lane.
  HeapVector<Member<GridLanesItemData>> items_densely_packed_above;
};

// Stores items placed in a single grid lane.
struct GridLaneData : public GarbageCollected<GridLaneData> {
  void AddItem(GridLanesItemData* item) {
    item_data.push_back(std::move(item));
  }

  void Trace(Visitor* visitor) const { visitor->Trace(item_data); }

  // Whether any item that starts in this lane still needs to finish layout.
  // Non-start spanner entries are owned by their start lane and do not affect
  // this state.
  bool has_unfinished_items = false;
  HeapVector<Member<GridLanesItemData>> item_data;
};

using GridLanesDataVector = HeapVector<Member<GridLaneData>, 1>;

// Returns the item at `index_path` in `lane_data`'s item tree. The first index
// selects an item from `GridLaneData::item_data`. Each subsequent index selects
// an item from the previously selected item's `items_densely_packed_above`.
// For example, {2, 1, 0} selects root item 2, then its packed child 1, then
// that child's packed child 0. An empty path represents no item.
GridLanesItemData* GridLanesItemDataFromPath(const GridLaneData& lane_data,
                                             const ItemIndexPath& index_path);

// Adds an item entry to every lane occupied by its span.
//
// For a densely packed item, `parent_item_index_path_per_lane` has one path per
// occupied lane. Each path identifies the item below the selected opening in
// that lane. An empty path means there is no item below in that lane, so the
// item is appended at the root. `parent_item_index_path_per_lane` will be
// completely empty for a non-densely-packed item.
void AddItemToGridLanesData(
    GridItemData& grid_lanes_item,
    GridLanesItemPlacementData* grid_lanes_placement_data,
    const Vector<ItemIndexPath>& parent_item_index_path_per_lane,
    GridTrackSizingDirection grid_axis_direction,
    GridLanesDataVector& out_grid_lanes);

// Returns the placement data at `item_index_path`. Returns null when
// `grid_lanes` is not provided.
GridLanesItemPlacementData* FindGridLanesItemPlacementData(
    const GridItemData& item,
    GridTrackSizingDirection grid_axis_direction,
    const ItemIndexPath& item_index_path,
    const GridLanesDataVector* grid_lanes);

// Applies an offset adjustment once to each shared item placement record.
void AdjustGridLanesItemPlacementOffsets(LayoutUnit offset_adjustment,
                                         bool is_block_direction,
                                         GridLanesDataVector& grid_lanes);

// Reverses the direct and packed item order within each lane.
void ReverseGridLanesItemOrder(GridLanesDataVector& grid_lanes);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANE_DATA_H_
