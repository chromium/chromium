// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/grid/grid_node.h"

#include "third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/grid/grid_placement.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"

namespace blink {

GridItems GridNode::ConstructGridItems(
    const GridLineResolver& line_resolver,
    bool* must_invalidate_placement_cache,
    HeapVector<Member<LayoutBox>>* opt_oof_children,
    bool* opt_has_nested_subgrid) const {
  return ConstructGridItems(line_resolver, /*root_grid_style=*/Style(),
                            /*parent_grid_style=*/Style(),
                            line_resolver.HasStandaloneAxis(kForColumns),
                            line_resolver.HasStandaloneAxis(kForRows),
                            must_invalidate_placement_cache, opt_oof_children,
                            opt_has_nested_subgrid);
}

GridItems GridNode::ConstructGridItems(
    const GridLineResolver& line_resolver,
    const ComputedStyle& root_grid_style,
    const ComputedStyle& parent_grid_style,
    bool must_consider_grid_items_for_column_sizing,
    bool must_consider_grid_items_for_row_sizing,
    bool* must_invalidate_placement_cache,
    HeapVector<Member<LayoutBox>>* opt_oof_children,
    bool* opt_has_nested_subgrid) const {
  DCHECK(must_invalidate_placement_cache);

  if (opt_has_nested_subgrid) {
    *opt_has_nested_subgrid = false;
  }

  GridItems grid_items;
  auto* layout_grid = To<LayoutGrid>(box_.Get());
  const GridPlacementData* cached_placement_data = nullptr;

  if (layout_grid->HasCachedPlacementData()) {
    cached_placement_data = &layout_grid->CachedPlacementData();

    // Even if the cached placement data is incorrect, as long as the grid is
    // not marked as dirty, the grid item count should be the same.
    grid_items.ReserveInitialCapacity(
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

      auto grid_item = std::make_unique<GridItemData>(
          To<BlockNode>(child), parent_grid_style, root_grid_style,
          must_consider_grid_items_for_column_sizing,
          must_consider_grid_items_for_row_sizing);

      // We'll need to sort when we encounter a non-initial order property.
      should_sort_grid_items_by_order_property |=
          child.Style().Order() != initial_order;

      // Check whether we'll need to further append subgridded items.
      if (opt_has_nested_subgrid) {
        *opt_has_nested_subgrid |= grid_item->IsSubgrid();
      }
      grid_items.Append(std::move(grid_item));
    }

    if (should_sort_grid_items_by_order_property)
      grid_items.SortByOrderProperty();
  }

#if DCHECK_IS_ON()
  if (cached_placement_data) {
    GridPlacement grid_placement(Style(), line_resolver);
    DCHECK(*cached_placement_data ==
           grid_placement.RunAutoPlacementAlgorithm(grid_items));
  }
#endif

  if (!cached_placement_data) {
    GridPlacement grid_placement(Style(), line_resolver);
    layout_grid->SetCachedPlacementData(
        grid_placement.RunAutoPlacementAlgorithm(grid_items));
    cached_placement_data = &layout_grid->CachedPlacementData();
  }

  // Copy each resolved position to its respective grid item data.
  auto resolved_position = cached_placement_data->grid_item_positions.begin();
  for (auto& grid_item : grid_items) {
    grid_item.resolved_position = *(resolved_position++);
  }
  return grid_items;
}

void GridNode::AppendSubgriddedItems(GridItems* grid_items) const {
  DCHECK(grid_items);

  const auto& root_grid_style = Style();
  for (wtf_size_t i = 0; i < grid_items->Size(); ++i) {
    auto& current_item = grid_items->At(i);

    if (!current_item.must_consider_grid_items_for_column_sizing &&
        !current_item.must_consider_grid_items_for_row_sizing) {
      continue;
    }

    bool must_invalidate_placement_cache = false;
    const auto subgrid = To<GridNode>(current_item.node);

    auto subgridded_items = subgrid.ConstructGridItems(
        subgrid.CachedLineResolver(), root_grid_style, subgrid.Style(),
        current_item.must_consider_grid_items_for_column_sizing,
        current_item.must_consider_grid_items_for_row_sizing,
        &must_invalidate_placement_cache);

    DCHECK(!must_invalidate_placement_cache)
        << "We shouldn't need to invalidate the placement cache if we relied "
           "on the cached line resolver; it must produce the same placement.";

    auto TranslateSubgriddedItem =
        [&current_item](GridSpan& subgridded_item_span,
                        GridTrackSizingDirection track_direction) {
          if (current_item.MustConsiderGridItemsForSizing(track_direction)) {
            // If a subgrid is in an opposite writing direction to the root
            // grid, we should "reverse" the subgridded item's span.
            if (current_item.IsOppositeDirectionInRootGrid(track_direction)) {
              const wtf_size_t subgrid_span_size =
                  current_item.SpanSize(track_direction);

              DCHECK_LE(subgridded_item_span.EndLine(), subgrid_span_size);

              subgridded_item_span = GridSpan::TranslatedDefiniteGridSpan(
                  subgrid_span_size - subgridded_item_span.EndLine(),
                  subgrid_span_size - subgridded_item_span.StartLine());
            }
            subgridded_item_span.Translate(
                current_item.StartLine(track_direction));
          }
        };

    for (auto& subgridded_item : subgridded_items) {
      subgridded_item.is_subgridded_to_parent_grid = true;
      auto& item_position = subgridded_item.resolved_position;

      if (!current_item.is_parallel_with_root_grid) {
        std::swap(item_position.columns, item_position.rows);
      }

      TranslateSubgriddedItem(item_position.columns, kForColumns);
      TranslateSubgriddedItem(item_position.rows, kForRows);
    }
    grid_items->Append(&subgridded_items);
  }
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
