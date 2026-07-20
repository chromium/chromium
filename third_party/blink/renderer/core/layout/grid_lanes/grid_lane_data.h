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

// Item and placement data for a single grid lanes item. If the item is a
// spanner and dense packing is enabled, this will also store any items that
// were packed above it.
struct GridLanesItemData : public GarbageCollected<GridLanesItemData> {
  GridLanesItemData(GridItemData* item,
                    const GridItemPlacementData& placement_data,
                    bool is_item_start = true)
      : item(item),
        placement_data(placement_data),
        is_item_start(is_item_start) {}

  void AddPackedItem(GridLanesItemData* packed_item) {
    items_packed_above.push_back(std::move(packed_item));
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(item);
    visitor->Trace(items_packed_above);
  }

  Member<GridItemData> item;
  GridItemPlacementData placement_data;

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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANE_DATA_H_
