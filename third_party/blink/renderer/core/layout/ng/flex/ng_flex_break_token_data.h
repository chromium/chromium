// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_line.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token_data.h"

namespace blink {

struct NGFlexBreakTokenData final : NGBlockBreakTokenData {
  NGFlexBreakTokenData(const NGBlockBreakTokenData* break_token_data,
                       const HeapVector<NGFlexLine>& flex_lines,
                       const Vector<EBreakBetween>& row_break_between,
                       const HeapVector<Member<LayoutBox>>& oof_children,
                       LayoutUnit intrinsic_block_size,
                       bool broke_before_row)
      : NGBlockBreakTokenData(kFlexBreakTokenData, break_token_data),
        flex_lines(flex_lines),
        row_break_between(row_break_between),
        oof_children(oof_children),
        intrinsic_block_size(intrinsic_block_size),
        broke_before_row(broke_before_row) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(flex_lines);
    visitor->Trace(oof_children);
    NGBlockBreakTokenData::Trace(visitor);
  }

  HeapVector<NGFlexLine> flex_lines;
  Vector<EBreakBetween> row_break_between;
  HeapVector<Member<LayoutBox>> oof_children;
  LayoutUnit intrinsic_block_size;
  // |broke_before_row| is only used in the case of row flex containers. If this
  // is true, that means that the next row to be processed had broken before,
  // as represented by a break before its first child.
  bool broke_before_row = false;
};

template <>
struct DowncastTraits<NGFlexBreakTokenData> {
  static bool AllowFrom(const NGBlockBreakTokenData& token_data) {
    return token_data.IsFlexType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_BREAK_TOKEN_DATA_H_
