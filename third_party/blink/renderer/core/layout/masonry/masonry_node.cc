// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_node.h"

#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_line_resolver.h"

namespace blink {

namespace {

void AdjustMasonryItemSpan(GridItemData& masonry_item,
                           const GridLineResolver& line_resolver,
                           const GridTrackSizingDirection grid_axis_direction) {
  // Resolve the positions of the items based on style. We can only resolve
  // the number of spans for each item based on the grid axis.
  GridSpan item_span = line_resolver.ResolveGridPositionsFromStyle(
      masonry_item.node.Style(), grid_axis_direction);

  if (item_span.IsIndefinite()) {
    masonry_item.is_auto_placed = true;
  }

  masonry_item.resolved_position.SetSpan(item_span, grid_axis_direction);
}

}  // namespace

MasonryItemGroups MasonryNode::CollectItemGroups(
    const GridLineResolver& line_resolver,
    const GridItems& masonry_items,
    wtf_size_t& max_end_line,
    wtf_size_t& start_offset,
    wtf_size_t& unplaced_item_span_count) const {
  const auto grid_axis_direction = Style().MasonryTrackSizingDirection();

  start_offset = 0;
  MasonryItemGroupMap item_group_map;

  for (wtf_size_t index = 0; index < masonry_items.Size(); ++index) {
    const Member<GridItemData>& masonry_item = masonry_items[index];
    DCHECK(masonry_item);

    const BlockNode& child = masonry_item->node;
    if (child.IsOutOfFlowPositioned()) {
      continue;
    }

    const auto item_properties = MasonryItemGroupProperties(
        /*item_span=*/line_resolver.ResolveGridPositionsFromStyle(
            child.Style(), grid_axis_direction));

    const auto& item_span = item_properties.Span();
    // Keep a running sum of unplaced item spans to determine where to
    // place auto placed virtual items per the auto-fit masonry heuristic.
    //
    // https://drafts.csswg.org/css-grid-3/#repeat-auto-fit
    if (item_span.IsIndefinite()) {
      unplaced_item_span_count += item_span.SpanSize();
    }
    if (item_span.IsUntranslatedDefinite()) {
      start_offset =
          std::max<int>(start_offset, -item_span.UntranslatedStartLine());
    }

    const auto group_it = item_group_map.find(item_properties);
    if (group_it == item_group_map.end()) {
      item_group_map.insert(item_properties,
                            GridItems::GridItemDataVector({masonry_item}));
    } else {
      group_it->value.emplace_back(masonry_item);
    }
  }

  MasonryItemGroups item_groups;
  item_groups.ReserveInitialCapacity(item_group_map.size());
  max_end_line =
      start_offset + line_resolver.ExplicitGridTrackCount(grid_axis_direction);

  for (auto& [group_properties, group_items] : item_group_map) {
    auto item_span = group_properties.Span();

    if (item_span.IsIndefinite()) {
      max_end_line = std::max(max_end_line, item_span.IndefiniteSpanSize());
    } else {
      DCHECK(item_span.IsUntranslatedDefinite());
      item_span.Translate(start_offset);
      max_end_line = std::max(max_end_line, item_span.EndLine());
    }

    item_groups.emplace_back(MasonryItemGroup{
        std::move(group_items), MasonryItemGroupProperties(item_span)});
  }
  return item_groups;
}

GridItems MasonryNode::ConstructMasonryItems(
    const GridLineResolver& line_resolver,
    HeapVector<Member<LayoutBox>>* opt_oof_children) const {
  const ComputedStyle& style = Style();
  const GridTrackSizingDirection grid_axis_direction =
      style.MasonryTrackSizingDirection();

  GridItems masonry_items;
  {
    bool should_sort_masonry_items_by_order_property = false;
    const int initial_order = ComputedStyleInitialValues::InitialOrder();

    // This collects all our children, and orders them by their order property.
    for (auto child = FirstChild(); child; child = child.NextSibling()) {
      if (child.IsOutOfFlowPositioned()) {
        if (opt_oof_children) {
          opt_oof_children->emplace_back(child.GetLayoutBox());
        }
        continue;
      }

      GridItemData* masonry_item = MakeGarbageCollected<GridItemData>(
          To<BlockNode>(child), /*parent_style=*/style);

      // We'll need to sort when we encounter a non-initial order property.
      should_sort_masonry_items_by_order_property |=
          child.Style().Order() != initial_order;

      AdjustMasonryItemSpan(*masonry_item, line_resolver, grid_axis_direction);
      masonry_items.Append(masonry_item);
    }

    // Sort items by order property if needed.
    if (should_sort_masonry_items_by_order_property) {
      masonry_items.SortByOrderProperty();
    }
  }
  return masonry_items;
}

void MasonryNode::AdjustMasonryItemSpans(
    GridItems& masonry_items,
    const GridLineResolver& line_resolver) const {
  const GridTrackSizingDirection grid_axis_direction =
      Style().MasonryTrackSizingDirection();
  for (GridItemData& masonry_item : masonry_items) {
    AdjustMasonryItemSpan(masonry_item, line_resolver, grid_axis_direction);
  }
}

}  // namespace blink
