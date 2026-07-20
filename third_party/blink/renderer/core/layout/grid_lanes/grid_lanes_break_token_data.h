// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/break_token_algorithm_data.h"

namespace blink {

struct GridLanesBreakTokenData final : BreakTokenAlgorithmData {
  GridLanesBreakTokenData(LayoutUnit intrinsic_block_size)
      : BreakTokenAlgorithmData(kGridLanesData),
        intrinsic_block_size(intrinsic_block_size) {}

  // TODO(almaher): Create and store new data structure holding grid lanes
  // item offsets per track, including subgrid info and any dense packing info.

  LayoutUnit intrinsic_block_size;
};

template <>
struct DowncastTraits<GridLanesBreakTokenData> {
  static bool AllowFrom(const BreakTokenAlgorithmData& token_data) {
    return token_data.IsGridLanesType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_BREAK_TOKEN_DATA_H_
