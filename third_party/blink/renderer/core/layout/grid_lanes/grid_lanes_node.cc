// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_node.h"

#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_line_resolver.h"
#include "third_party/blink/renderer/core/style/grid_position.h"

namespace blink {

namespace {

void AdjustGridItemSpan(GridItemData& grid_lanes_item,
                        const GridLineResolver& line_resolver,
                        const GridTrackSizingDirection grid_axis_direction) {
  // Resolve the positions of the items based on style. We can only resolve
  // the number of spans for each item based on the grid axis.
  GridSpan item_span = line_resolver.ResolveGridPositionsFromStyle(
      grid_lanes_item.node.Style(), grid_axis_direction);

  if (item_span.IsIndefinite()) {
    grid_lanes_item.is_auto_placed = true;
  }

  grid_lanes_item.resolved_position.SetSpan(item_span, grid_axis_direction);
}

}  // namespace

GridLanesItemGroups GridLanesNode::CollectItemGroups(
    const GridLineResolver& line_resolver,
    const GridItems& grid_lanes_items,
    wtf_size_t& max_end_line,
    wtf_size_t& start_offset,
    wtf_size_t& unplaced_item_span_count) const {
  const auto grid_axis_direction = Style().GridLanesTrackSizingDirection();

  start_offset = 0;
  GridLanesItemGroupMap item_group_map;

  for (wtf_size_t index = 0; index < grid_lanes_items.Size(); ++index) {
    const Member<GridItemData>& grid_lanes_item = grid_lanes_items[index];
    DCHECK(grid_lanes_item);

    const BlockNode& child = grid_lanes_item->node;
    if (child.IsOutOfFlowPositioned()) {
      continue;
    }

    // Determine baseline-sharing group for this item.
    std::optional<BaselineGroup> baseline_group = std::nullopt;
    if (grid_lanes_item->IsBaselineAligned(grid_axis_direction)) {
      baseline_group = grid_lanes_item->BaselineGroup(grid_axis_direction);
    }

    // Subgridded items use their `resolved_position` which was already adjusted
    // to parent coordinates by `AdjustSubgriddedItemSpan`.
    const GridSpan item_span =
        grid_lanes_item->is_subgridded_to_parent_grid
            ? grid_lanes_item->Span(grid_axis_direction)
            : line_resolver.ResolveGridPositionsFromStyle(
                  grid_lanes_item->node.Style(), grid_axis_direction);

    const auto item_properties =
        GridLanesItemGroupProperties(item_span, baseline_group);

    // Keep a running sum of unplaced item spans to determine where to
    // place auto placed virtual items per the auto-fit grid-lanes heuristic.
    //
    // https://drafts.csswg.org/css-grid-3/#repeat-auto-fit
    if (item_span.IsIndefinite()) {
      unplaced_item_span_count += item_span.SpanSize();
    }

    // Subgridded items don't contribute to `start_offset` since their spans
    // are already in the parent's translated coordinate space.
    if (item_span.IsUntranslatedDefinite()) {
      CHECK(!grid_lanes_item->is_subgridded_to_parent_grid);
      start_offset =
          std::max<int>(start_offset, -item_span.UntranslatedStartLine());
    }

    const auto group_it = item_group_map.find(item_properties);
    if (group_it == item_group_map.end()) {
      item_group_map.insert(item_properties,
                            GridItems::GridItemDataVector({grid_lanes_item}));
    } else {
      group_it->value.emplace_back(grid_lanes_item);
    }
  }

  GridLanesItemGroups item_groups;
  item_groups.ReserveInitialCapacity(item_group_map.size());
  max_end_line =
      start_offset + line_resolver.ExplicitGridTrackCount(grid_axis_direction);

  for (auto& [group_properties, group_items] : item_group_map) {
    auto item_span = group_properties.Span();

    if (item_span.IsIndefinite()) {
      max_end_line = std::max(max_end_line, item_span.IndefiniteSpanSize());
    } else {
      if (item_span.IsUntranslatedDefinite()) {
        item_span.Translate(start_offset);
      }
      max_end_line = std::max(max_end_line, item_span.EndLine());
    }

    item_groups.emplace_back(MakeGarbageCollected<GridLanesItemGroup>(
        std::move(group_items),
        GridLanesItemGroupProperties(item_span,
                                     group_properties.GetBaselineGroup())));
  }
  return item_groups;
}

// TODO(almaher): Similar to grid, we should eventually create an overloaded
// method that takes `must_consider_for_columns` and `must_consider_for_rows`
// so that we can pass that in for grid lanes subgrids.
GridItems* GridLanesNode::ConstructGridItems(
    const GridLineResolver& line_resolver,
    bool* must_invalidate_placement_cache,
    HeapVector<Member<LayoutBox>>* opt_oof_children,
    bool* opt_has_nested_subgrid) const {
  const ComputedStyle& style = Style();
  const GridTrackSizingDirection grid_axis_direction =
      style.GridLanesTrackSizingDirection();

  // For grid-lanes, we only consider subgridding in the grid axis.
  const bool must_consider_for_columns = (grid_axis_direction == kForColumns);
  const bool must_consider_for_rows = (grid_axis_direction == kForRows);

  if (opt_has_nested_subgrid) {
    *opt_has_nested_subgrid = false;
  }

  GridItems* grid_lanes_items = MakeGarbageCollected<GridItems>();
  {
    bool should_sort_grid_lanes_items_by_order_property = false;
    const int initial_order = ComputedStyleInitialValues::InitialOrder();

    // This collects all our children, and orders them by their order property.
    for (auto child = FirstChild(); child; child = child.NextSibling()) {
      if (child.IsOutOfFlowPositioned()) {
        if (opt_oof_children) {
          opt_oof_children->emplace_back(child.GetLayoutBox());
        }
        continue;
      }

      GridItemData* grid_lanes_item = MakeGarbageCollected<GridItemData>(
          To<BlockNode>(child), /*parent_grid_style=*/style,
          /*root_grid_style=*/style, must_consider_for_columns,
          must_consider_for_rows);

      // We'll need to sort when we encounter a non-initial order property.
      should_sort_grid_lanes_items_by_order_property |=
          child.Style().Order() != initial_order;

      // Check whether we'll need to further append subgridded items.
      if (opt_has_nested_subgrid) {
        *opt_has_nested_subgrid |= grid_lanes_item->IsSubgrid();
      }

      AdjustGridItemSpan(*grid_lanes_item, line_resolver, grid_axis_direction);
      grid_lanes_items->Append(grid_lanes_item);
    }

    // Sort items by order property if needed.
    if (should_sort_grid_lanes_items_by_order_property) {
      grid_lanes_items->SortByOrderProperty();
    }
  }
  return grid_lanes_items;
}

void GridLanesNode::AdjustSubgriddedItemSpan(
    const GridItemData& subgrid_item,
    GridItemData& subgridded_item) const {
  const auto grid_axis_direction = Style().GridLanesTrackSizingDirection();

  // In grid lanes, subgridding only occurs in the grid axis.
  CHECK(subgrid_item.MustConsiderGridItemsForSizing(grid_axis_direction));

  auto& subgridded_span = (grid_axis_direction == kForColumns)
                              ? subgridded_item.resolved_position.columns
                              : subgridded_item.resolved_position.rows;

  // If the subgrid is auto-placed or the subgridded item has an indefinite
  // span, the item is treated as auto placed, contributing to every possible
  // parent grid lanes track.
  if (subgrid_item.is_auto_placed || subgridded_span.IsIndefinite()) {
    subgridded_span = GridSpan::IndefiniteGridSpan(subgridded_span.SpanSize());
    subgridded_item.is_auto_placed = true;
  } else {
    // Both the subgrid and the subgridded item have definite positions.
    // Translate the subgridded item's span to the parent grid's coordinate
    // space, constrained to the subgrid's actual track range.
    if (subgrid_item.IsOppositeDirectionInRootGrid(grid_axis_direction)) {
      // If a subgrid is in an opposite writing direction to the root
      // grid, we should "reverse" the subgridded item's span.
      const wtf_size_t subgrid_span_size =
          subgrid_item.SpanSize(grid_axis_direction);
      DCHECK_LE(subgridded_span.EndLine(), subgrid_span_size);
      subgridded_span = GridSpan::TranslatedDefiniteGridSpan(
          subgrid_span_size - subgridded_span.EndLine(),
          subgrid_span_size - subgridded_span.StartLine());
    }
    subgridded_span.Translate(subgrid_item.StartLine(grid_axis_direction));
  }
}

void GridLanesNode::ComputeSetIndicesForSubgrid(
    GridItemData& subgrid_item,
    GridLayoutData& layout_data) const {
  // In grid lanes, placement happens after sizing, so the placement of subgrid
  // items may not be known at this point. Translate definite spans using the
  // `start_offset` cached by BuildSizingCollection. For items without a known
  // position, assume they start at the beginning of the explicit grid.
  //
  // TODO(almaher): We may need to do an additional pass for row grid-lanes
  // containers, or if items depend on the block size constraint in column
  // grid-lanes, to ensure we get the correct position for these subgrids, as
  // that can impact subgridded item contributions and thus track sizing.
  const auto grid_axis = Style().GridLanesTrackSizingDirection();
  const wtf_size_t start_offset = CachedPlacementData().StartOffset(grid_axis);

  auto& span = (grid_axis == kForColumns)
                   ? subgrid_item.resolved_position.columns
                   : subgrid_item.resolved_position.rows;

  if (span.IsUntranslatedDefinite()) {
    span.Translate(start_offset);
  } else if (span.IsIndefinite()) {
    span = GridSpan::TranslatedDefiniteGridSpan(start_offset,
                                                start_offset + span.SpanSize());
  }

  // Grid-lanes only has tracks in the grid axis.
  if (grid_axis == kForColumns) {
    subgrid_item.ComputeSetIndices(layout_data.Columns());
  } else {
    subgrid_item.ComputeSetIndices(layout_data.Rows());
  }
}

// TODO(almaher): We may be able to optimize this by caching the largest span
// size when children are added to `LayoutGridLanes`, but this would require
// extra invalidation logic, which, given that we only need this in certain
// scoped cases at the moment, would end up being more expensive in the total.
wtf_size_t GridLanesNode::ComputeLargestChildSpanSize() const {
  const ComputedStyle& style = Style();
  const auto grid_axis_direction = style.GridLanesTrackSizingDirection();
  wtf_size_t largest_span = 0;

  // The largest span size may be inaccurate if it depends on line names or
  // numbers, as the final span size requires knowing the full number of auto
  // repeats. We use 1 auto repeat as a heuristic here to get a "reasonable"
  // estimate.
  const GridLineResolver temp_line_resolver(style, 1u);
  for (auto child = FirstChild(); child; child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned()) {
      continue;
    }

    const ComputedStyle& child_style = child.Style();
    GridSpan item_span = temp_line_resolver.ResolveGridPositionsFromStyle(
        child_style, grid_axis_direction);

    largest_span = std::max(largest_span, item_span.SpanSize());
  }

  return largest_span;
}

}  // namespace blink
