// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_item_iterator.h"

#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"

namespace blink {

NGFlexItemIterator::NGFlexItemIterator(const Vector<FlexLine>& flex_lines,
                                       const NGBlockBreakToken* break_token)
    : flex_lines_(flex_lines), break_token_(break_token) {
  if (flex_lines_.size()) {
    DCHECK(flex_lines_[0].line_items_.size());
    next_unstarted_item_ =
        const_cast<FlexItem*>(&flex_lines_[0].line_items_[0]);
    flex_item_idx_++;
  }
  if (break_token_) {
    const auto& child_break_tokens = break_token_->ChildBreakTokens();
    // If there are child break tokens, we don't yet know which one is the
    // next unstarted item (need to get past the child break tokens first). If
    // we've already seen all children, there will be no unstarted items.
    if (!child_break_tokens.empty() || break_token_->HasSeenAllChildren()) {
      next_unstarted_item_ = nullptr;
      flex_item_idx_ = 0;
    }
    // We're already done with this parent break token if there are no child
    // break tokens, so just forget it right away.
    if (child_break_tokens.empty())
      break_token_ = nullptr;
  }
}

NGFlexItemIterator::Entry NGFlexItemIterator::NextItem() {
  const NGBlockBreakToken* current_child_break_token = nullptr;
  FlexItem* current_item = next_unstarted_item_;

  if (break_token_) {
    // If we're resuming layout after a fragmentainer break, we'll first resume
    // the items that fragmented earlier (represented by one break token
    // each).
    DCHECK(!next_unstarted_item_);
    const auto& child_break_tokens = break_token_->ChildBreakTokens();

    if (child_token_idx_ < child_break_tokens.size()) {
      current_child_break_token =
          To<NGBlockBreakToken>(child_break_tokens[child_token_idx_++].Get());
      current_item = FindNextItem(current_child_break_token);

      if (child_token_idx_ == child_break_tokens.size()) {
        // We reached the last child break token. Prepare for the next unstarted
        // sibling, and forget the parent break token.
        if (!break_token_->HasSeenAllChildren())
          next_unstarted_item_ = FindNextItem();
        break_token_ = nullptr;
      }
    }
  } else if (next_unstarted_item_) {
    next_unstarted_item_ = FindNextItem();
  }

  return Entry(current_item, current_child_break_token);
}

FlexItem* NGFlexItemIterator::FindNextItem(
    const NGBlockBreakToken* item_break_token) {
  while (flex_line_idx_ < flex_lines_.size()) {
    const auto& flex_line = flex_lines_[flex_line_idx_];
    while (flex_item_idx_ < flex_line.line_items_.size()) {
      FlexItem* flex_item =
          const_cast<FlexItem*>(&flex_line.line_items_[flex_item_idx_++]);
      if (!item_break_token ||
          flex_item->ng_input_node_ == item_break_token->InputNode())
        return flex_item;
    }
    flex_item_idx_ = 0;
    flex_line_idx_++;
  }
  DCHECK(!item_break_token);
  return nullptr;
}

}  // namespace blink
