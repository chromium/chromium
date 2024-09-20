// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_NG_FLEX_LINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_NG_FLEX_LINE_H_

#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/geometry/flex_offset.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct NGFlexItem {
  DISALLOW_NEW();

 public:
  NGFlexItem() : ng_input_node(nullptr) {}

  const ComputedStyle& Style() const { return ng_input_node.Style(); }

  void Trace(Visitor* visitor) const { visitor->Trace(ng_input_node); }

  LayoutUnit main_axis_final_size;
  LayoutUnit margin_block_end;
  // This will originally be set to the total block size of the item before
  // fragmentation. It will then be reduced while performing fragmentation. If
  // it becomes negative, that means that the item expanded as a result of
  // fragmentation. This is only used for column flex containers.
  LayoutUnit total_remaining_block_size;
  FlexOffset offset;
  bool is_initial_block_size_indefinite = false;
  bool is_used_flex_basis_indefinite = false;
  bool has_descendant_that_depends_on_percentage_block_size = false;
  BlockNode ng_input_node;
};

struct NGFlexLine {
  DISALLOW_NEW();

 public:
  explicit NGFlexLine(wtf_size_t num_items) : line_items(num_items) {}

  LayoutUnit LineCrossEnd() const {
    return line_cross_size + cross_axis_offset + item_offset_adjustment;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(line_items); }

  LayoutUnit main_axis_free_space;
  LayoutUnit line_cross_size;
  LayoutUnit cross_axis_offset;
  LayoutUnit major_baseline;
  LayoutUnit minor_baseline;
  LayoutUnit item_offset_adjustment;
  bool has_seen_all_children = false;
  HeapVector<NGFlexItem> line_items;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGFlexItem)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGFlexLine)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_NG_FLEX_LINE_H_
