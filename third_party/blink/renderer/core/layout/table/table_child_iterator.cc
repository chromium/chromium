// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/table_child_iterator.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"

namespace blink {

TableChildIterator::TableChildIterator(
    const TableGroupedChildren& grouped_children,
    const BlockBreakToken* break_token)
    : grouped_children_(&grouped_children), break_token_(break_token) {
  if (break_token_) {
    const auto& child_break_tokens = break_token_->ChildBreakTokens();
    if (child_break_tokens.empty()) {
      // There are no nodes to resume...
      if (break_token_->HasSeenAllChildren()) {
        // ...and we have seen all children. This means that we have no work
        // left to do.
        grouped_children_ = nullptr;
        return;
      } else {
        // ...but we haven't seen all children yet. This means that we need to
        // start at the beginning.
        break_token_ = nullptr;
      }
    }
  }

  if (grouped_children_->captions.size()) {
    // Find the first top caption, if any.
    while (caption_idx_ < grouped_children_->captions.size()) {
      if (grouped_children_->captions[caption_idx_].Style().CaptionSide() ==
          ECaptionSide::kTop)
        return;
      caption_idx_++;
    }
    // Didn't find a top caption. Prepare for looking for bottom captions, once
    // we're through the section iterator.
    caption_idx_ = 0;
  }

  // Start the section iterator.
  section_iterator_.emplace(grouped_children_->begin());
}

TableChildIterator::Entry TableChildIterator::NextChild() {
  const BlockBreakToken* current_child_break_token = nullptr;
  BlockNode current_child(nullptr);

  if (break_token_) {
    const auto& child_break_tokens = break_token_->ChildBreakTokens();
    if (child_token_idx_ < child_break_tokens.size()) {
      current_child_break_token =
          To<BlockBreakToken>(child_break_tokens[child_token_idx_++].Get());
      current_child = To<BlockNode>(current_child_break_token->InputNode());

      // Normally (for non-tables), when we're out of break tokens, we can
      // just proceed to the next sibling node, but we can't do this for
      // tables, since the captions and sections get reordered as: top
      // captions, table header, table bodies, table footer, bottom captions.
      // Also keep track of the section index as we advance.
      while (CurrentChild() != current_child) {
        AdvanceChild();
        DCHECK(CurrentChild());
      }

      if (child_token_idx_ == child_break_tokens.size()) {
        // We reached the last child break token. Proceed with the next
        // unstarted child, unless we've already seen all children (in which
        // case we're done).
        if (break_token_->HasSeenAllChildren())
          grouped_children_ = nullptr;
        break_token_ = nullptr;
      }
    }
  } else {
    current_child = CurrentChild();
  }

  wtf_size_t current_section_idx = section_idx_;
  AdvanceChild();

  return Entry(current_child, current_child_break_token, current_section_idx);
}

BlockNode TableChildIterator::CurrentChild() const {
  if (!grouped_children_)
    return BlockNode(nullptr);  // We have nothing.

  if (!section_iterator_) {
    // We're at a top caption, since we have no iterator yet.
    DCHECK_EQ(grouped_children_->captions[caption_idx_].Style().CaptionSide(),
              ECaptionSide::kTop);
    return grouped_children_->captions[caption_idx_];
  }

  if (*section_iterator_ != grouped_children_->end()) {
    // We're at a table section.
    return **section_iterator_;
  }

  if (caption_idx_ < grouped_children_->captions.size()) {
    // We're at a bottom caption, since the iterator is at end().
    DCHECK_EQ(grouped_children_->captions[caption_idx_].Style().CaptionSide(),
              ECaptionSide::kBottom);
    return grouped_children_->captions[caption_idx_];
  }

  // We're done.
  return BlockNode(nullptr);
}

void TableChildIterator::AdvanceChild() {
  if (!grouped_children_)
    return;
  if (!section_iterator_) {
    // We're currently at a top caption. See if there are more of them.
    caption_idx_++;
    while (caption_idx_ < grouped_children_->captions.size()) {
      if (grouped_children_->captions[caption_idx_].Style().CaptionSide() ==
          ECaptionSide::kTop)
        return;
      caption_idx_++;
    }

    // We're done with the top captions, but we'll go through the captions
    // vector again after the table sections, to look for bottom captions.
    caption_idx_ = 0;

    // But first we need to look for sections.
    DCHECK(!section_iterator_);
    section_iterator_.emplace(grouped_children_->begin());
    if (*section_iterator_ != grouped_children_->end())
      return;  // Found a section.

    // No sections. Proceed to bottom captions.
  } else {
    if (*section_iterator_ != grouped_children_->end()) {
      // Go to the next section, if any.
      ++(*section_iterator_);
      section_idx_++;
      if (*section_iterator_ != grouped_children_->end())
        return;  // Found another section.
      // No more sections. Proceed to bottom captions.
    } else {
      // Go to the the next bottom caption, if any.
      caption_idx_++;
    }
  }

  while (caption_idx_ < grouped_children_->captions.size()) {
    if (grouped_children_->captions[caption_idx_].Style().CaptionSide() ==
        ECaptionSide::kBottom)
      return;
    caption_idx_++;
  }
}

}  // namespace blink
