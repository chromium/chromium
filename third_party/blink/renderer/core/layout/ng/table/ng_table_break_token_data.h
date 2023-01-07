// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token_data.h"

namespace blink {

struct NGTableBreakTokenData final : NGBlockBreakTokenData {
  NGTableBreakTokenData(const NGBlockBreakTokenData* break_token_data,
                        LayoutUnit consumed_table_box_block_size,
                        bool has_entered_table_box,
                        bool is_past_table_box)
      : NGBlockBreakTokenData(kTableBreakTokenData, break_token_data),
        consumed_table_box_block_size(consumed_table_box_block_size),
        has_entered_table_box(has_entered_table_box),
        is_past_table_box(is_past_table_box) {}

  // Similar to |consumed_block_size|, but it only counts space consumed by the
  // table box itself, i.e. excluding any captions.
  LayoutUnit consumed_table_box_block_size;

  // True if we have entered the table box, i.e. we're past any top captions.
  bool has_entered_table_box;

  // True if we're past the table box, i.e. all sections (or, if there are no
  // sections, this will be set once we've added all the space required by
  // specified block-size & co).
  bool is_past_table_box;
};

template <>
struct DowncastTraits<NGTableBreakTokenData> {
  static bool AllowFrom(const NGBlockBreakTokenData& token_data) {
    return token_data.IsTableType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_BREAK_TOKEN_DATA_H_
