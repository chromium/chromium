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

struct GridItemData;
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
  // resolves the grid line positions of the items based on style. If
  // `oof_children` is provided, aggregate any out of flow children.
  // `must_invalidate_placement_cache` isn't used in grid-lanes because
  // the placement cache is populated at a later point in grid-lanes, and
  // placement also happens after track sizing in grid-lanes, so the placement
  // cache isn't as heavily relied on for performance with subgrid as it is in
  // grid. However, we still need to include it in the signature for common
  // call sites with grid.
  //
  // `parent_is_auto_placed` is true when this grid is itself an auto-placed
  // subgrid inside a grid-lanes ancestor — i.e. the ancestor resolves its own
  // track positions after track sizing, so this subgrid's position in the
  // ancestor's tracks is unknown at sizing time. As such, any items within this
  // subgrid should also be considered auto-placed if true.
  GridItems* ConstructGridItems(
      const GridLineResolver& line_resolver,
      bool* must_invalidate_placement_cache,
      bool parent_is_auto_placed = false,
      HeapVector<Member<LayoutBox>>* opt_oof_children = nullptr,
      bool* opt_has_nested_subgrid = nullptr) const;

  // Adjusts a subgridded item's span to be relative to the parent grid's
  // coordinate system if the span is definite.
  void AdjustSubgriddedItemSpan(const GridItemData& subgrid_item,
                                GridItemData& subgridded_item) const;

  // Translates the subgrid item's span and computes its set indices.
  void ComputeSetIndicesForSubgrid(GridItemData& subgrid_item,
                                   GridLayoutData& layout_data) const;

  // Computes the largest span size among all children by examining their
  // grid placement styles directly. Note that this may be an inaccurate value
  // if any child's span size depends on line names or numbers, as the final
  // span size requires knowing the full number of auto repeats.
  wtf_size_t ComputeLargestChildSpanSize() const;
};

template <>
struct DowncastTraits<GridLanesNode> {
  static bool AllowFrom(const LayoutInputNode& node) {
    return node.IsGridLanes();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_NODE_H_
