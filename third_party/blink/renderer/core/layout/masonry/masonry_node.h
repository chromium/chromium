// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/masonry/layout_masonry.h"
#include "third_party/blink/renderer/core/layout/masonry/masonry_item_group.h"

namespace blink {

class GridItems;
class GridLineResolver;

// Masonry specific extensions to `BlockNode`.
class CORE_EXPORT MasonryNode final : public BlockNode {
 public:
  explicit MasonryNode(LayoutBox* box) : BlockNode(box) {
    DCHECK(box);
    DCHECK(box->IsLayoutMasonry());
  }

  const GridPlacementData& CachedPlacementData() const {
    return To<LayoutMasonry>(box_.Get())->CachedPlacementData();
  }

  // Collects the children of this node (using the `GridItemData` for each child
  // provided by `masonry_items`) into item groups based on their placement,
  // span size, and baseline-sharing group. `start_offset` calculates the offset
  // of the first grid line in the implicit grid, which is used to translate
  // definite grid spans to a 0-indexed format. `unplaced_item_span_count` is
  // an ouput param that is the sum of all auto placed item span sizes.
  MasonryItemGroups CollectItemGroups(
      const GridLineResolver& line_resolver,
      const GridItems& masonry_items,
      wtf_size_t& max_end_line,
      wtf_size_t& start_offset,
      wtf_size_t& unplaced_item_span_count) const;

  // Collects the children of this node, sorts by order property if needed, and
  // resolves the grid line positions of the items based on style.
  GridItems ConstructMasonryItems(
      const GridLineResolver& line_resolver,
      HeapVector<Member<LayoutBox>>* opt_oof_children = nullptr) const;

  // Update the grid line positions of the items based on style and provided
  // `line_resolver`.
  void AdjustMasonryItemSpans(GridItems& masonry_items,
                              const GridLineResolver& line_resolver) const;
};

template <>
struct DowncastTraits<MasonryNode> {
  static bool AllowFrom(const LayoutInputNode& node) {
    return node.IsMasonry();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_NODE_H_
