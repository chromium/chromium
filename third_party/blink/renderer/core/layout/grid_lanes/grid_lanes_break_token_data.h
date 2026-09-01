// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/break_token_algorithm_data.h"
#include "third_party/blink/renderer/core/layout/grid/grid_data.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lane_data.h"

namespace blink {

struct GridLanesBreakTokenData final : BreakTokenAlgorithmData {
  GridLanesBreakTokenData(const GridLanesDataVector& grid_lanes,
                          const GridLayoutSubtree* grid_layout_subtree,
                          LayoutUnit total_intrinsic_block_size,
                          const HeapVector<Member<LayoutBox>>& oof_children)
      : BreakTokenAlgorithmData(kGridLanesData),
        total_intrinsic_block_size(total_intrinsic_block_size),
        grid_layout_subtree(grid_layout_subtree),
        grid_lanes(grid_lanes),
        oof_children(oof_children) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(grid_lanes);
    visitor->Trace(grid_layout_subtree);
    visitor->Trace(oof_children);
    BreakTokenAlgorithmData::Trace(visitor);
  }

  LayoutUnit total_intrinsic_block_size;

  Member<const GridLayoutSubtree> grid_layout_subtree;
  GridLanesDataVector grid_lanes;

  HeapVector<Member<LayoutBox>> oof_children;
};

template <>
struct DowncastTraits<GridLanesBreakTokenData> {
  static bool AllowFrom(const BreakTokenAlgorithmData& token_data) {
    return token_data.IsGridLanesType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_BREAK_TOKEN_DATA_H_
