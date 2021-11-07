// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_line.h"

namespace blink {

struct NGFlexBreakTokenData {
  NGFlexBreakTokenData(const Vector<NGFlexLine>& flex_lines,
                       LayoutUnit intrinsic_block_size)
      : flex_lines(flex_lines), intrinsic_block_size(intrinsic_block_size) {}

  Vector<NGFlexLine> flex_lines;
  LayoutUnit intrinsic_block_size;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_BREAK_TOKEN_DATA_H_
