// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_node.h"

#include "third_party/blink/renderer/core/layout/grid/grid_line_resolver.h"

namespace blink {

MasonryItemGroups MasonryNode::CollectItemGroups(
    const GridLineResolver& line_resolver,
    wtf_size_t* start_offset) const {
  DCHECK(start_offset);

  *start_offset = 0;
  MasonryItemGroups item_groups;
  const auto grid_axis_direction = Style().MasonryTrackSizingDirection();

  for (auto child = FirstChild(); child; child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned()) {
      continue;
    }

    const auto item_properties = MasonryItemGroupProperties(
        /*item_span=*/line_resolver.ResolveGridPositionsFromStyle(
            child.Style(), grid_axis_direction));

    const auto& item_span = item_properties.Span();
    if (!item_span.IsIndefinite()) {
      DCHECK(item_span.IsUntranslatedDefinite());
      *start_offset =
          std::max<int>(*start_offset, -item_span.UntranslatedStartLine());
    }

    const auto group_it = item_groups.find(item_properties);
    if (group_it == item_groups.end()) {
      item_groups.insert(item_properties,
                         HeapVector<BlockNode, 16>({To<BlockNode>(child)}));
    } else {
      group_it->value.emplace_back(To<BlockNode>(child));
    }
  }
  return item_groups;
}

}  // namespace blink
