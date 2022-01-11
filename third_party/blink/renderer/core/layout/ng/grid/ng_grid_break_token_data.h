// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token_data.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct GridItemOffsets {
  GridItemOffsets(const LogicalOffset offset,
                  const LogicalOffset relative_offset)
      : offset(offset), relative_offset(relative_offset) {}

  LogicalOffset offset;
  LogicalOffset relative_offset;
};

struct NGGridBreakTokenData final : NGBlockBreakTokenData {
  USING_FAST_MALLOC(NGGridBreakTokenData);

 public:
  NGGridBreakTokenData(const NGBlockBreakTokenData* break_token_data,
                       const NGGridGeometry& grid_geometry,
                       const Vector<GridItemOffsets>& offsets,
                       const Vector<LayoutUnit>& row_offset_adjustments,
                       const Vector<EBreakBetween>& row_break_between,
                       LayoutUnit intrinsic_block_size)
      : NGBlockBreakTokenData(kGridBreakTokenData, break_token_data),
        grid_geometry(grid_geometry),
        offsets(offsets),
        row_offset_adjustments(row_offset_adjustments),
        row_break_between(row_break_between),
        intrinsic_block_size(intrinsic_block_size) {}

  NGGridGeometry grid_geometry;
  Vector<GridItemOffsets> offsets;
  Vector<LayoutUnit> row_offset_adjustments;
  Vector<EBreakBetween> row_break_between;
  LayoutUnit intrinsic_block_size;
};

template <>
struct DowncastTraits<NGGridBreakTokenData> {
  static bool AllowFrom(const NGBlockBreakTokenData& token_data) {
    return token_data.IsGridType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_BREAK_TOKEN_DATA_H_
