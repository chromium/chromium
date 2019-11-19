// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_child_iterator.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

namespace blink {

NGBlockChildIterator::NGBlockChildIterator(NGLayoutInputNode first_child,
                                           const NGBlockBreakToken* break_token)
    : child_(first_child), break_token_(break_token), child_token_idx_(0) {}

NGBlockChildIterator::Entry NGBlockChildIterator::NextChild(
    const NGInlineBreakToken* previous_inline_break_token) {
  const NGBreakToken* child_break_token = nullptr;
  if (previous_inline_break_token &&
      !previous_inline_break_token->IsFinished()) {
    return Entry(previous_inline_break_token->InputNode(),
                 previous_inline_break_token);
  }

  if (break_token_) {
    // If we're resuming layout after a fragmentainer break, we'll first resume
    // the children that fragmented earlier (represented by one break token
    // each).
    const auto& child_break_tokens = break_token_->ChildBreakTokens();

    while (child_token_idx_ < child_break_tokens.size()) {
      child_break_token = child_break_tokens[child_token_idx_++];
      // While it never happens to blocks, line boxes may produce break tokens
      // even if we're finished. And those we just ignore.
      if (!child_break_token->IsFinished())
        break;
    }
    // If there are no break tokens left to resume, the iterator machinery (see
    // further below) will just continue at the next sibling. The last break
    // token would be the last node that got fragmented. However, there may be
    // parallel flows caused by visible overflow, established by descendants of
    // our children, and these may go on, fragmentainer after fragmentainer,
    // even if we're done with our direct children. When this happens, we need
    // to prevent the machinery from continuing iterating, if we're already done
    // with those siblings.
    if (!child_break_token && break_token_->HasSeenAllChildren())
      child_ = nullptr;
  }

  // If we have a child break token to resume at, that's the source of truth.
  if (child_break_token)
    child_ = child_break_token->InputNode();

  NGLayoutInputNode child = child_;
  if (child_)
    child_ = child_.NextSibling();

  return Entry(child, child_break_token);
}

}  // namespace blink
