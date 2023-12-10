// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/flex_item_iterator.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/flex/ng_flex_line.h"

namespace blink {

FlexItemIterator::FlexItemIterator(const HeapVector<NGFlexLine>& flex_lines,
                                   const BlockBreakToken* break_token,
                                   bool is_column)
    : flex_lines_(flex_lines),
      break_token_(break_token),
      is_column_(is_column) {
  if (flex_lines_.size()) {
    DCHECK(flex_lines_[0].line_items.size());
    next_unstarted_item_ =
        const_cast<NGFlexItem*>(&flex_lines_[0].line_items[0]);
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

FlexItemIterator::Entry FlexItemIterator::NextItem(bool broke_before_row) {
  DCHECK(!is_column_ || !broke_before_row);

  const BlockBreakToken* current_child_break_token = nullptr;
  NGFlexItem* current_item = next_unstarted_item_;
  wtf_size_t current_item_idx = 0;
  wtf_size_t current_line_idx = 0;

  if (break_token_) {
    // If we're resuming layout after a fragmentainer break, we'll first resume
    // the items that fragmented earlier (represented by one break token
    // each).
    DCHECK(!next_unstarted_item_);
    const auto& child_break_tokens = break_token_->ChildBreakTokens();

    if (child_token_idx_ < child_break_tokens.size()) {
      current_child_break_token =
          To<BlockBreakToken>(child_break_tokens[child_token_idx_++].Get());
      DCHECK(current_child_break_token);
      current_item = FindNextItem(current_child_break_token);

      if (is_column_) {
        while (next_item_idx_for_line_.size() <= flex_line_idx_)
          next_item_idx_for_line_.push_back(0);
        // Store the next item index to process for this column so that the
        // remaining items can be processed after the break tokens have been
        // handled.
        next_item_idx_for_line_[flex_line_idx_] = flex_item_idx_;
      }

      current_item_idx = flex_item_idx_ - 1;
      current_line_idx = flex_line_idx_;

      if (child_token_idx_ == child_break_tokens.size()) {
        // We reached the last child break token. Prepare for the next unstarted
        // sibling, and forget the parent break token.
        if (!is_column_ && (current_item_idx != 0 ||
                            !current_child_break_token->IsBreakBefore() ||
                            !broke_before_row)) {
          // All flex items in a row are processed before moving to the next
          // fragmentainer, unless the row broke before. If the current item in
          // the row has a break token, but the next item in the row doesn't,
          // that means the next item has already finished layout. In this case,
          // move to the next row.
          //
          // Note: Rows don't produce a layout result, so if the row broke
          // before, the first item in the row will have a broken before.
          break_token_ = nullptr;
          NextLine();
        } else if (!break_token_->HasSeenAllChildren()) {
          if (is_column_) {
            // Re-iterate over the columns to find any unprocessed items.
            flex_line_idx_ = 0;
            flex_item_idx_ = next_item_idx_for_line_[flex_line_idx_];
          }
          next_unstarted_item_ = FindNextItem();
          break_token_ = nullptr;
        }
      }
    }
  } else {
    current_item_idx = flex_item_idx_ - 1;
    current_line_idx = flex_line_idx_;
    if (next_unstarted_item_)
      next_unstarted_item_ = FindNextItem();
  }

  return Entry(current_item, current_item_idx, current_line_idx,
               current_child_break_token);
}

NGFlexItem* FlexItemIterator::FindNextItem(
    const BlockBreakToken* item_break_token) {
  while (flex_line_idx_ < flex_lines_.size()) {
    const auto& flex_line = flex_lines_[flex_line_idx_];
    if (!flex_line.has_seen_all_children || item_break_token) {
      while (flex_item_idx_ < flex_line.line_items.size()) {
        NGFlexItem* flex_item =
            const_cast<NGFlexItem*>(&flex_line.line_items[flex_item_idx_++]);
        if (!item_break_token ||
            flex_item->ng_input_node == item_break_token->InputNode())
          return flex_item;
      }
    }
    // If the current column had a break token, but later columns do not, that
    // means that those later columns have completed layout and can be skipped.
    if (is_column_ && !item_break_token &&
        flex_line_idx_ == next_item_idx_for_line_.size() - 1)
      break;

    flex_line_idx_++;
    AdjustItemIndexForNewLine();
  }

  // We handle break tokens for all columns before moving to the unprocessed
  // items for each column. This means that we may process a break token in an
  // earlier column after a break token in a later column. Thus, if we haven't
  // found the item matching the current break token, re-iterate from the first
  // column.
  if (item_break_token) {
    DCHECK(is_column_);
    flex_line_idx_ = 0;
    flex_item_idx_ = next_item_idx_for_line_[flex_line_idx_];
    return FindNextItem(item_break_token);
  }
  return nullptr;
}

void FlexItemIterator::NextLine() {
  if (flex_item_idx_ == 0)
    return;
  flex_line_idx_++;
  AdjustItemIndexForNewLine();
  if (!break_token_)
    next_unstarted_item_ = FindNextItem();
}

void FlexItemIterator::AdjustItemIndexForNewLine() {
  if (flex_line_idx_ < next_item_idx_for_line_.size())
    flex_item_idx_ = next_item_idx_for_line_[flex_line_idx_];
  else
    flex_item_idx_ = 0;
}

}  // namespace blink
