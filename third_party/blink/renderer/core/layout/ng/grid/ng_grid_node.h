// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

namespace blink {

// Grid specific extensions to NGBlockNode.
class CORE_EXPORT NGGridNode final : public NGBlockNode {
 public:
  explicit NGGridNode(LayoutBox* box) : NGBlockNode(box) {
    DCHECK(box && box->IsLayoutNGGrid());
  }

  const NGGridPlacementData& CachedPlacementData() const;

  // If |oof_children| is provided, aggregate any out of flow children.
  GridItems ConstructGridItems(const NGGridPlacementData& placement_data,
                               HeapVector<Member<LayoutBox>>* oof_children,
                               bool* has_nested_subgrid) const;

  void AppendSubgriddedItems(GridItems* grid_items) const;

 private:
  GridItems ConstructGridItems(
      const NGGridPlacementData& placement_data,
      const ComputedStyle& root_grid_style,
      bool must_consider_grid_items_for_column_sizing,
      bool must_consider_grid_items_for_row_sizing,
      HeapVector<Member<LayoutBox>>* oof_children = nullptr,
      bool* has_nested_subgrid = nullptr) const;
};

template <>
struct DowncastTraits<NGGridNode> {
  static bool AllowFrom(const NGLayoutInputNode& node) { return node.IsGrid(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_NODE_H_
