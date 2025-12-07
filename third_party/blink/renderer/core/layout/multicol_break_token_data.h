// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MULTICOL_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MULTICOL_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/break_token_algorithm_data.h"

namespace blink {

struct MulticolBreakTokenData final : BreakTokenAlgorithmData {
  explicit MulticolBreakTokenData(LayoutUnit consumed_row_block_size)
      : BreakTokenAlgorithmData(kMulticolData),
        consumed_row_block_size(consumed_row_block_size) {}

  // In nested block fragmentation, when a column row (specified by the
  // `column-height` property) is too tall to fit in one outer fragmentainer,
  // the remainder needs to be handled in subsequent outer fragmentainers.
  LayoutUnit consumed_row_block_size;
};

template <>
struct DowncastTraits<MulticolBreakTokenData> {
  static bool AllowFrom(const BreakTokenAlgorithmData& token_data) {
    return token_data.IsMulticolType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MULTICOL_BREAK_TOKEN_DATA_H_
