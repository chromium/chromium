// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_node.h"

#include "third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/grid/grid_placement.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"

namespace blink {

GridItems* GridNode::ConstructGridItems(
    const GridLineResolver& line_resolver,
    bool* must_invalidate_placement_cache,
    bool parent_is_auto_placed,
    HeapVector<Member<LayoutBox>>* opt_oof_children,
    bool* opt_has_nested_subgrid) const {
  return ConstructGridItems(line_resolver, /*root_grid_style=*/Style(),
                            /*parent_grid_style=*/Style(),
                            line_resolver.HasStandaloneAxis(kForColumns),
                            line_resolver.HasStandaloneAxis(kForRows),
                            must_invalidate_placement_cache,
                            parent_is_auto_placed, opt_oof_children,
                            opt_has_nested_subgrid);
}

GridItems* GridNode::ConstructGridItems(
    const GridLineResolver& line_resolver,
    const ComputedStyle& root_grid_style,
    const ComputedStyle& parent_grid_style,
    bool must_consider_grid_items_for_column_sizing,
    bool must_consider_grid_items_for_row_sizing,
    bool* must_invalidate_placement_cache,
    bool parent_is_auto_placed,
    HeapVector<Member<LayoutBox>>* opt_oof_children,
    bool* opt_has_nested_subgrid) const {
  DCHECK(must_invalidate_placement_cache);

  if (opt_has_nested_subgrid) {
    *opt_has_nested_subgrid = false;
  }

  GridItems* grid_items = MakeGarbageCollected<GridItems>();
  auto* layout_grid = To<LayoutGrid>(box_.Get());
  const GridPlacementData* cached_placement_data = nullptr;

  if (layout_grid->HasCachedPlacementData()) {
    cached_placement_data = &layout_grid->CachedPlacementData();

    // Even if the cached placement data is incorrect, as long as the grid is
    // not marked as dirty, the grid item count should be the same.
    grid_items->ReserveInitialCapacity(
        cached_placement_data->grid_item_positions.size());

    if (*must_invalidate_placement_cache ||
        line_resolver != cached_placement_data->line_resolver) {
      // We need to recompute grid item placement if the automatic column/row
      // repetitions changed due to updates in the container's style or if any
      // grid in the ancestor chain invalidated its subtree's placement cache.
      cached_placement_data = nullptr;
    }
  }

  // Placement cache gets invalidated when there are significant changes in this
  // grid's computed style. However, these changes might alter the placement of
  // subgridded items, so this flag is used to signal that we need to recurse
  // into subgrids to recompute their placement.
  *must_invalidate_placement_cache |= !cached_placement_data;

  {
    bool should_sort_grid_items_by_order_property = false;
    const int initial_order = ComputedStyleInitialValues::InitialOrder();

    for (auto child = FirstChild(); child; child = child.NextSibling()) {
      if (child.IsOutOfFlowPositioned()) {
        if (opt_oof_children) {
          opt_oof_children->emplace_back(child.GetLayoutBox());
        }
        continue;
      }

      auto* grid_item = MakeGarbageCollected<GridItemData>(
          To<BlockNode>(child), parent_grid_style, root_grid_style,
          must_consider_grid_items_for_column_sizing,
          must_consider_grid_items_for_row_sizing);

      // If this grid is itself an auto-placed subgrid (e.g. inside a
      // grid-lanes ancestor whose placement happens after track sizing), then
      // any child's final position in the grid-lanes ancestor's tracks is
      // unknown — even children with explicit placement, because their
      // explicit placement is only relative to this subgrid and this
      // subgrid's own position is unresolved. Mark these child items as
      // auto-placed as a result.
      if (parent_is_auto_placed) {
        grid_item->is_auto_placed = true;
      }

      // We'll need to sort when we encounter a non-initial order property.
      should_sort_grid_items_by_order_property |=
          child.Style().Order() != initial_order;

      // Check whether we'll need to further append subgridded items.
      if (opt_has_nested_subgrid) {
        *opt_has_nested_subgrid |= grid_item->IsSubgrid();
      }
      grid_items->Append(grid_item);
    }

    if (should_sort_grid_items_by_order_property) {
      grid_items->SortByOrderProperty();
    }
  }

#if DCHECK_IS_ON()
  if (cached_placement_data) {
    GridPlacement grid_placement(Style(), line_resolver);
    DCHECK(*cached_placement_data ==
           grid_placement.RunAutoPlacementAlgorithm(*grid_items));
  }
#endif

  if (!cached_placement_data) {
    GridPlacement grid_placement(Style(), line_resolver);
    layout_grid->SetCachedPlacementData(
        grid_placement.RunAutoPlacementAlgorithm(*grid_items));
    cached_placement_data = &layout_grid->CachedPlacementData();
  }

  // Copy each resolved position to its respective grid item data.
  auto resolved_position =
      base::span(cached_placement_data->grid_item_positions).begin();
  for (auto& grid_item : *grid_items) {
    grid_item.resolved_position = *(resolved_position++);
  }
  return grid_items;
}

void GridNode::AdjustSubgriddedItemSpan(const GridItemData& subgrid_item,
                                        GridItemData& subgridded_item) const {
  // Translate the subgridded item's spans from the subgrid's coordinate space
  // to the parent grid's coordinate space.
  auto& item_position = subgridded_item.resolved_position;

  // If the surrounding subgrid is itself auto-placed (e.g. inside a
  // grid-lanes ancestor whose placement happens after track sizing), this
  // subgridded item's final position in the grid-lanes ancestor's tracks is
  // unknown regardless of its own placement. Mark it as "auto-placed".
  if (subgrid_item.is_auto_placed) {
    subgridded_item.is_auto_placed = true;
  }

  auto TranslateSpan = [&subgrid_item](GridSpan& span,
                                       GridTrackSizingDirection direction) {
    if (subgrid_item.MustConsiderGridItemsForSizing(direction)) {
      // If a subgrid is in an opposite writing direction to the root
      // grid, we should "reverse" the subgridded item's span.
      if (subgrid_item.IsOppositeDirectionInRootGrid(direction)) {
        const wtf_size_t subgrid_span_size = subgrid_item.SpanSize(direction);
        DCHECK_LE(span.EndLine(), subgrid_span_size);
        span = GridSpan::TranslatedDefiniteGridSpan(
            subgrid_span_size - span.EndLine(),
            subgrid_span_size - span.StartLine());
      }
      span.Translate(subgrid_item.StartLine(direction));
    }
  };

  TranslateSpan(item_position.columns, kForColumns);
  TranslateSpan(item_position.rows, kForRows);
}

void GridNode::ComputeSetIndicesForSubgrid(GridItemData& subgrid_item,
                                           GridLayoutData& layout_data) const {
  subgrid_item.ComputeSetIndices(layout_data.Columns());
  subgrid_item.ComputeSetIndices(layout_data.Rows());
}

MinMaxSizesResult GridNode::ComputeSubgridMinMaxSizes(
    const GridSizingSubtree& sizing_subtree,
    const ConstraintSpace& space) const {
  DCHECK(sizing_subtree.HasValidRootFor(*this));
  DCHECK(sizing_subtree.LayoutData().IsSubgridWithStandaloneAxis(kForColumns));

  auto* layout_grid = To<LayoutGrid>(box_.Get());

  if (!layout_grid->HasCachedSubgridMinMaxSizes()) {
    const auto fragment_geometry = CalculateInitialFragmentGeometry(
        space, *this, /*break_token=*/nullptr, /*is_intrinsic=*/true);

    layout_grid->SetSubgridMinMaxSizesCache(
        GridLayoutAlgorithm({*this, fragment_geometry, space})
            .ComputeSubgridMinMaxSizes(sizing_subtree),
        sizing_subtree.LayoutData());
  }

  return {layout_grid->CachedSubgridMinMaxSizes(),
          /*depends_on_block_constraints=*/false};
}

LayoutUnit GridNode::ComputeSubgridIntrinsicBlockSize(
    const GridSizingSubtree& sizing_subtree,
    const ConstraintSpace& space) const {
  DCHECK(sizing_subtree.HasValidRootFor(*this));
  DCHECK(sizing_subtree.LayoutData().IsSubgridWithStandaloneAxis(kForRows));

  auto* layout_grid = To<LayoutGrid>(box_.Get());

  if (!layout_grid->HasCachedSubgridMinMaxSizes()) {
    const auto fragment_geometry = CalculateInitialFragmentGeometry(
        space, *this, /*break_token=*/nullptr, /*is_intrinsic=*/true);

    const auto intrinsic_block_size =
        GridLayoutAlgorithm({*this, fragment_geometry, space})
            .ComputeSubgridIntrinsicBlockSize(sizing_subtree);

    // The min and max-content block size are both the box's "ideal" size after
    // layout (see https://drafts.csswg.org/css-sizing-3/#max-content).
    layout_grid->SetSubgridMinMaxSizesCache(
        {intrinsic_block_size, intrinsic_block_size},
        sizing_subtree.LayoutData());
  }

  // Both intrinsic sizes are the same, so we can return either.
  return layout_grid->CachedSubgridMinMaxSizes().max_size;
}

}  // namespace blink
