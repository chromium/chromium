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

  // Start of the item's opening in forward stacking order. Self-alignment and
  // fill-reverse may move the item without changing this position.
  LayoutUnit forward_stacking_start;

  // Unique item identifier within a placement pass. All lane entries for a
  // spanner share this value. It also breaks ties between equal final
  // `CrossGap` centers.
  wtf_size_t placement_sequence = 0;

  // Index of the item's fragment in the container builder during normal layout.
  // Unset during fragmentation collection.
  wtf_size_t builder_child_index = kNotFound;
};

// Item and placement data for a single grid lanes item. If the item is a
// spanner and dense packing is enabled, this will also store any items that
// were packed above it. Entries for the same spanner share
// `grid_lanes_placement_data`, while `is_item_start` and
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

  wtf_size_t PlacementSequence() const {
    return grid_lanes_placement_data->placement_sequence;
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

  // Only set if this item is a spanner with items densely packed above it.
  HeapVector<Member<GridLanesItemData>> items_densely_packed_above;
};

// Stores items placed in a single grid lane.
struct GridLaneData : public GarbageCollected<GridLaneData> {
  void AddItem(GridLanesItemData* item) {
    item_data.push_back(std::move(item));
  }

  void Trace(Visitor* visitor) const { visitor->Trace(item_data); }

  // Whether every item that starts in this lane has finished layout. A spanner
  // is owned by the first lane it occupies, so it only impacts this state in
  // that lane.
  bool has_seen_all_children = false;
  HeapVector<Member<GridLanesItemData>> item_data;
};

using GridLanesDataVector = HeapVector<Member<GridLaneData>, 1>;

// Adds an item entry to every lane occupied by its span.
//
// For a densely packed item, `item_indices_below_opening` has one index per
// occupied lane. Each index identifies the item below the selected opening.
// `kNotFound` indicates that there is no item below the opening, so the new
// entry is added directly to the lane. The vector is empty for an item that was
// not densely packed.
void AddItemToGridLanesData(
    GridItemData& grid_lanes_item,
    GridLanesItemPlacementData* grid_lanes_placement_data,
    const Vector<wtf_size_t>& item_indices_below_opening,
    GridTrackSizingDirection grid_axis_direction,
    GridLanesDataVector& out_grid_lanes);

// Returns the placement data for `item`. `item_index` identifies either the
// item itself or the lane entry below its opening when it was densely packed.
// Returns null when `grid_lanes` is not provided.
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
