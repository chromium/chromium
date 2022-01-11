// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_line.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token_data.h"

namespace blink {

struct NGFlexBreakTokenData final : NGBlockBreakTokenData {
  NGFlexBreakTokenData(const NGBlockBreakTokenData* break_token_data,
                       const Vector<NGFlexLine>& flex_lines,
                       LayoutUnit intrinsic_block_size)
      : NGBlockBreakTokenData(kFlexBreakTokenData, break_token_data),
        flex_lines(flex_lines),
        intrinsic_block_size(intrinsic_block_size) {}

  Vector<NGFlexLine> flex_lines;
  LayoutUnit intrinsic_block_size;
};

template <>
struct DowncastTraits<NGFlexBreakTokenData> {
  static bool AllowFrom(const NGBlockBreakTokenData& token_data) {
    return token_data.IsFlexType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_BREAK_TOKEN_DATA_H_
