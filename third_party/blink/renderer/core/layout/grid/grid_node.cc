// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_node.h"

#include "third_party/blink/renderer/core/layout/grid/grid_placement.h"

namespace blink {

GridItems GridNode::ConstructGridItems(
    const GridLineResolver& line_resolver,
    HeapVector<Member<LayoutBox>>* oof_children,
    bool* has_nested_subgrid) const {
  return ConstructGridItems(line_resolver, /* root_grid_style */ Style(),
                            /* parent_grid_style */ Style(),
                            line_resolver.HasStandaloneAxis(kForColumns),
                            line_resolver.HasStandaloneAxis(kForRows),
                            oof_children, has_nested_subgrid);
}

GridItems GridNode::ConstructGridItems(
    const GridLineResolver& line_resolver,
    const ComputedStyle& root_grid_style,
    const ComputedStyle& parent_grid_style,
    bool must_consider_grid_items_for_column_sizing,
    bool must_consider_grid_items_for_row_sizing,
    HeapVector<Member<LayoutBox>>* oof_children,
    bool* has_nested_subgrid) const {
  if (has_nested_subgrid)
    *has_nested_subgrid = false;

  GridItems grid_items;
  auto* layout_grid = To<LayoutGrid>(box_.Get());
  const GridPlacementData* cached_placement_data = nullptr;

  if (layout_grid->HasCachedPlacementData()) {
    cached_placement_data = &layout_grid->CachedPlacementData();

    // Even if the cached placement data is incorrect, as long as the grid is
    // not marked as dirty, the grid item count should be the same.
    grid_items.ReserveInitialCapacity(
        cached_placement_data->grid_item_positions.size());

    if (line_resolver != cached_placement_data->line_resolver) {
      // We need to recompute grid item placement if the automatic column/row
      // repetitions changed due to updates in the container's style.
      cached_placement_data = nullptr;
    }
  }

  {
    bool should_sort_grid_items_by_order_property = false;
    const int initial_order = ComputedStyleInitialValues::InitialOrder();

    for (auto child = FirstChild(); child; child = child.NextSibling()) {
      if (child.IsOutOfFlowPositioned()) {
        if (oof_children)
          oof_children->emplace_back(child.GetLayoutBox());
        continue;
      }

      auto grid_item = std::make_unique<GridItemData>(
          To<BlockNode>(child), root_grid_style,
          parent_grid_style.GetFontBaseline(),
          must_consider_grid_items_for_column_sizing,
          must_consider_grid_items_for_row_sizing);

      // We'll need to sort when we encounter a non-initial order property.
      should_sort_grid_items_by_order_property |=
          child.Style().Order() != initial_order;

      // Check whether we'll need to further append subgridded items.
      if (has_nested_subgrid)
        *has_nested_subgrid |= grid_item->IsSubgrid();

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
  auto* resolved_position = cached_placement_data->grid_item_positions.begin();
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

    const auto subgrid = To<GridNode>(current_item.node);

    auto subgridded_items = subgrid.ConstructGridItems(
        subgrid.CachedLineResolver(), root_grid_style, subgrid.Style(),
        current_item.must_consider_grid_items_for_column_sizing,
        current_item.must_consider_grid_items_for_row_sizing);

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

}  // namespace blink
