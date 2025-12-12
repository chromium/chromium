// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_item_group.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/layout_grid_lanes.h"

namespace blink {

class GridItems;
class GridLineResolver;

// Grid Lanes specific extensions to `BlockNode`.
class CORE_EXPORT GridLanesNode final : public BlockNode {
 public:
  explicit GridLanesNode(LayoutBox* box) : BlockNode(box) {
    DCHECK(box);
    DCHECK(box->IsLayoutGridLanes());
  }

  const GridPlacementData& CachedPlacementData() const {
    return To<LayoutGridLanes>(box_.Get())->CachedPlacementData();
  }

  // Collects the children of this node (using the `GridItemData` for each child
  // provided by `grid_lanes_items`) into item groups based on their placement,
  // span size, and baseline-sharing group. `start_offset` calculates the offset
  // of the first grid line in the implicit grid, which is used to translate
  // definite grid spans to a 0-indexed format. `unplaced_item_span_count` is
  // an ouput param that is the sum of all auto placed item span sizes.
  GridLanesItemGroups CollectItemGroups(
      const GridLineResolver& line_resolver,
      const GridItems& grid_lanes_items,
      wtf_size_t& max_end_line,
      wtf_size_t& start_offset,
      wtf_size_t& unplaced_item_span_count) const;

  // Collects the children of this node, sorts by order property if needed, and
  // resolves the grid line positions of the items based on style.
  GridItems ConstructGridLanesItems(
      const GridLineResolver& line_resolver,
      HeapVector<Member<LayoutBox>>* opt_oof_children = nullptr) const;

  // Update the grid line positions of the items based on style and provided
  // `line_resolver`.
  void AdjustGridLanesItemSpans(GridItems& grid_lanes_items,
                                const GridLineResolver& line_resolver) const;
};

template <>
struct DowncastTraits<GridLanesNode> {
  static bool AllowFrom(const LayoutInputNode& node) {
    return node.IsGridLanes();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_NODE_H_
