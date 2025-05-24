// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_node.h"

#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_line_resolver.h"

namespace blink {

MasonryItemGroups MasonryNode::CollectItemGroups(
    const GridLineResolver& line_resolver,
    wtf_size_t& max_end_line,
    wtf_size_t& start_offset) const {
  const auto grid_axis_direction = Style().MasonryTrackSizingDirection();

  start_offset = 0;
  MasonryItemGroupMap item_group_map;

  for (auto child = FirstChild(); child; child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned()) {
      continue;
    }

    const auto item_properties = MasonryItemGroupProperties(
        /*item_span=*/line_resolver.ResolveGridPositionsFromStyle(
            child.Style(), grid_axis_direction));

    const auto& item_span = item_properties.Span();
    if (item_span.IsUntranslatedDefinite()) {
      start_offset =
          std::max<int>(start_offset, -item_span.UntranslatedStartLine());
    }

    const auto group_it = item_group_map.find(item_properties);
    if (group_it == item_group_map.end()) {
      item_group_map.insert(item_properties,
                            HeapVector<BlockNode, 16>({To<BlockNode>(child)}));
    } else {
      group_it->value.emplace_back(To<BlockNode>(child));
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
    wtf_size_t start_offset) const {
  const auto& style = Style();
  const auto grid_axis_direction = style.MasonryTrackSizingDirection();

  GridItems masonry_items;
  {
    bool should_sort_masonry_items_by_order_property = false;
    const int initial_order = ComputedStyleInitialValues::InitialOrder();

    // This collects all our children, and orders them by their order property.
    for (auto child = FirstChild(); child; child = child.NextSibling()) {
      const auto& child_style = child.Style();
      auto* masonry_item = MakeGarbageCollected<GridItemData>(
          To<BlockNode>(child), /*parent_style=*/style);

      // We'll need to sort when we encounter a non-initial order property.
      should_sort_masonry_items_by_order_property |=
          child_style.Order() != initial_order;

      // Resolve the positions of the items based on style. We can only resolve
      // the number of spans for each item based on the grid axis.
      auto item_span = line_resolver.ResolveGridPositionsFromStyle(
          child_style, grid_axis_direction);

      if (item_span.IsUntranslatedDefinite()) {
        item_span.Translate(start_offset);
      }

      masonry_item->resolved_position.SetSpan(item_span, grid_axis_direction);
      masonry_items.Append(masonry_item);
    }

    // Sort items by order property if needed.
    if (should_sort_masonry_items_by_order_property) {
      masonry_items.SortByOrderProperty();
    }
  }
  return masonry_items;
}

}  // namespace blink
