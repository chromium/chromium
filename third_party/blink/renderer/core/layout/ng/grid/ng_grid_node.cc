// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_node.h"

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"

namespace blink {

const NGGridPlacementData& NGGridNode::CachedPlacementData() const {
  auto* layout_grid = To<LayoutNGGrid>(box_.Get());
  return layout_grid->CachedPlacementData();
}

GridItems NGGridNode::ConstructGridItems(
    const NGGridPlacementData& placement_data,
    bool* has_subgridded_items) const {
  DCHECK(has_subgridded_items);

  return ConstructGridItems(
      placement_data, Style(), placement_data.HasStandaloneAxis(kForColumns),
      placement_data.HasStandaloneAxis(kForRows), has_subgridded_items);
}

GridItems NGGridNode::ConstructGridItems(
    const NGGridPlacementData& placement_data,
    const ComputedStyle& root_grid_style,
    bool must_consider_grid_items_for_column_sizing,
    bool must_consider_grid_items_for_row_sizing,
    bool* has_subgridded_items) const {
  if (has_subgridded_items)
    *has_subgridded_items = false;

  GridItems grid_items;
  auto* layout_grid = To<LayoutNGGrid>(box_.Get());
  const NGGridPlacementData* cached_placement_data = nullptr;

  if (!layout_grid->IsGridPlacementDirty()) {
    cached_placement_data = &layout_grid->CachedPlacementData();

    // Even if the cached placement data is incorrect, as long as the grid is
    // not marked as dirty, the grid item count should be the same.
    grid_items.ReserveInitialCapacity(
        cached_placement_data->grid_item_positions.size());

    if (placement_data.column_auto_repetitions !=
            cached_placement_data->column_auto_repetitions ||
        placement_data.row_auto_repetitions !=
            cached_placement_data->row_auto_repetitions) {
      // We need to recompute grid item placement if the automatic column/row
      // repetitions changed due to updates in the container's style.
      cached_placement_data = nullptr;
    }
  }

  {
    bool should_sort_grid_items_by_order_property = false;
    const int initial_order = ComputedStyleInitialValues::InitialOrder();

    for (auto child = FirstChild(); child; child = child.NextSibling()) {
      if (child.IsOutOfFlowPositioned())
        continue;

      auto* grid_item = MakeGarbageCollected<GridItemData>(
          To<NGBlockNode>(child), root_grid_style,
          must_consider_grid_items_for_column_sizing,
          must_consider_grid_items_for_row_sizing);

      // We'll need to sort when we encounter a non-initial order property.
      should_sort_grid_items_by_order_property |=
          child.Style().Order() != initial_order;

      // Check whether we'll need to further append subgridded items.
      if (has_subgridded_items) {
        *has_subgridded_items |=
            grid_item->must_consider_grid_items_for_column_sizing ||
            grid_item->must_consider_grid_items_for_row_sizing;
      }
      grid_items.Append(grid_item);
    }

    if (should_sort_grid_items_by_order_property) {
      // Sort all of our in-flow children by their `order` property.
      auto CompareItemsByOrderProperty = [](const Member<GridItemData>& lhs,
                                            const Member<GridItemData>& rhs) {
        return lhs->node.Style().Order() < rhs->node.Style().Order();
      };
      std::stable_sort(grid_items.item_data.begin(), grid_items.item_data.end(),
                       CompareItemsByOrderProperty);
    }
  }

  const auto& grid_style = Style();
  const bool grid_is_parallel_with_root_grid = IsParallelWritingMode(
      root_grid_style.GetWritingMode(), grid_style.GetWritingMode());

#if DCHECK_IS_ON()
  if (cached_placement_data) {
    NGGridPlacement grid_placement(grid_style, placement_data);
    DCHECK(*cached_placement_data ==
           grid_placement.RunAutoPlacementAlgorithm(grid_items));
  }
#endif

  if (!cached_placement_data) {
    NGGridPlacement grid_placement(grid_style, placement_data);
    layout_grid->SetCachedPlacementData(
        grid_placement.RunAutoPlacementAlgorithm(grid_items));
    cached_placement_data = &layout_grid->CachedPlacementData();
  }

  // Copy each resolved position to its respective grid item data.
  auto* resolved_position = cached_placement_data->grid_item_positions.begin();
  for (auto& grid_item : grid_items) {
    grid_item.resolved_position = *(resolved_position++);

    if (!grid_is_parallel_with_root_grid) {
      std::swap(grid_item.resolved_position.columns,
                grid_item.resolved_position.rows);
    }
  }
  return grid_items;
}

GridItems NGGridNode::GridItemsIncludingSubgridded(
    const NGGridPlacementData& placement_data) const {
  bool has_subgridded_items;
  auto grid_items = ConstructGridItems(placement_data, &has_subgridded_items);

  if (!has_subgridded_items)
    return grid_items;

  const auto& root_grid_style = Style();
  for (wtf_size_t i = 0; i < grid_items.Size(); ++i) {
    auto& current_item = grid_items[i];

    if (!current_item.must_consider_grid_items_for_column_sizing &&
        !current_item.must_consider_grid_items_for_row_sizing) {
      continue;
    }

    const auto subgrid = To<NGGridNode>(current_item.node);
    NGGridPlacementData subgrid_placement_data(
        /* is_subgridded_to_parent */ true, subgrid.Style(),
        CachedPlacementData().line_resolver);

    if (current_item.has_subgridded_columns) {
      subgrid_placement_data.column_subgrid_span_size = current_item.SpanSize(
          current_item.is_parallel_with_root_grid ? kForColumns : kForRows);
    }

    if (current_item.has_subgridded_rows) {
      subgrid_placement_data.row_subgrid_span_size = current_item.SpanSize(
          current_item.is_parallel_with_root_grid ? kForRows : kForColumns);
    }

    // TODO(ethavar): Compute automatic repetitions for subgridded axes as
    // described in https://drafts.csswg.org/css-grid-2/#auto-repeat.

    auto subgridded_items = subgrid.ConstructGridItems(
        subgrid_placement_data, root_grid_style,
        current_item.must_consider_grid_items_for_column_sizing,
        current_item.must_consider_grid_items_for_row_sizing);

    grid_items.ReserveCapacity(grid_items.Size() + subgridded_items.Size());
    const wtf_size_t column_start_line = current_item.StartLine(kForColumns);
    const wtf_size_t row_start_line = current_item.StartLine(kForRows);

    for (auto& subgridded_item : subgridded_items) {
      subgridded_item.resolved_position.columns.Translate(column_start_line);
      subgridded_item.resolved_position.rows.Translate(row_start_line);
      subgridded_item.parent_grid = &current_item;
      grid_items.Append(&subgridded_item);
    }
  }
  return grid_items;
}

}  // namespace blink
