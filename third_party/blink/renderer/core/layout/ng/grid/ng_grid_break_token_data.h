// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_data.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct GridItemPlacementData {
  GridItemPlacementData(
      LogicalOffset offset,
      LogicalOffset relative_offset,
      bool has_descendant_that_depends_on_percentage_block_size)
      : offset(offset),
        relative_offset(relative_offset),
        has_descendant_that_depends_on_percentage_block_size(
            has_descendant_that_depends_on_percentage_block_size) {}

  LogicalOffset offset;
  LogicalOffset relative_offset;
  bool has_descendant_that_depends_on_percentage_block_size;
};

struct NGGridBreakTokenData final : NGBlockBreakTokenData {
  NGGridBreakTokenData(
      const NGBlockBreakTokenData* break_token_data,
      const NGGridLayoutData& layout_data,
      LayoutUnit intrinsic_block_size,
      LayoutUnit consumed_grid_block_size,
      Vector<GridItemIndices>&& column_range_indices,
      Vector<GridItemIndices>&& row_range_indices,
      Vector<GridArea>&& resolved_positions,
      const Vector<GridItemPlacementData>& grid_items_placement_data,
      const Vector<LayoutUnit>& row_offset_adjustments,
      const Vector<EBreakBetween>& row_break_between,
      const HeapVector<Member<LayoutBox>>& oof_children)
      : NGBlockBreakTokenData(kGridBreakTokenData, break_token_data),
        layout_data(layout_data),
        intrinsic_block_size(intrinsic_block_size),
        consumed_grid_block_size(consumed_grid_block_size),
        column_range_indices(std::move(column_range_indices)),
        row_range_indices(std::move(row_range_indices)),
        resolved_positions(std::move(resolved_positions)),
        grid_items_placement_data(grid_items_placement_data),
        row_offset_adjustments(row_offset_adjustments),
        row_break_between(row_break_between),
        oof_children(oof_children) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(oof_children);
    NGBlockBreakTokenData::Trace(visitor);
  }

  NGGridLayoutData layout_data;
  LayoutUnit intrinsic_block_size;

  // This is similar to |NGBlockBreakTokenData::consumed_block_size|, however
  // it isn't used for determining the final block-size of the fragment and
  // won't include any block-end padding (this prevents saturation bugs).
  LayoutUnit consumed_grid_block_size;
  Vector<GridItemIndices> column_range_indices;
  Vector<GridItemIndices> row_range_indices;
  Vector<GridArea> resolved_positions;
  Vector<GridItemPlacementData> grid_items_placement_data;
  Vector<LayoutUnit> row_offset_adjustments;
  Vector<EBreakBetween> row_break_between;
  HeapVector<Member<LayoutBox>> oof_children;
};

template <>
struct DowncastTraits<NGGridBreakTokenData> {
  static bool AllowFrom(const NGBlockBreakTokenData& token_data) {
    return token_data.IsGridType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_BREAK_TOKEN_DATA_H_
