// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LINE_H_

#include "third_party/blink/renderer/core/layout/geometry/flex_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct NGFlexItem {
  NGFlexItem() : ng_input_node(nullptr) {}

  const ComputedStyle& Style() const { return ng_input_node.Style(); }

  LayoutUnit main_axis_final_size;
  // This will originally be set to the total block size of the item before
  // fragmentation. It will then be reduced while performing fragmentation. If
  // it becomes negative, that means that the item expanded as a result of
  // fragmentation.
  LayoutUnit total_remaining_block_size;
  FlexOffset offset;
  NGBlockNode ng_input_node;
};

struct NGFlexLine {
  explicit NGFlexLine(wtf_size_t num_items) : line_items(num_items) {}

  LayoutUnit line_cross_size;
  LayoutUnit item_offset_adjustment;
  Vector<NGFlexItem> line_items;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LINE_H_
