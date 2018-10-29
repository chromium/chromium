// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_child_iterator.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

namespace blink {

NGBlockChildIterator::NGBlockChildIterator(NGLayoutInputNode first_child,
                                           const NGBlockBreakToken* break_token)
    : child_(first_child), break_token_(break_token), child_token_idx_(0) {
  // Locate the first child to resume layout at.
  if (!break_token)
    return;
  const auto& child_break_tokens = break_token->ChildBreakTokens();
  if (!child_break_tokens.size()) {
    if (!break_token->IsBreakBefore()) {
      // We had a break token, but no child break token, and we're not
      // at the start. This means that we're just processing empty
      // content, such as the trailing childless portion of a block
      // with specified height.
      child_ = nullptr;
    }
    return;
  }
  auto first_node_child = ToNGBlockNode(break_token->InputNode()).FirstChild();
  resuming_at_inline_formatting_context_ =
      first_node_child && first_node_child.IsInline();
  child_ = child_break_tokens[0]->InputNode();
}

NGBlockChildIterator::Entry NGBlockChildIterator::NextChild(
    const NGBreakToken* previous_inline_break_token) {
  const NGBreakToken* child_break_token = nullptr;

  if (previous_inline_break_token &&
      !previous_inline_break_token->IsFinished()) {
    DCHECK(previous_inline_break_token->IsInlineType());
    return Entry(previous_inline_break_token->InputNode(),
                 previous_inline_break_token);
  }

  if (break_token_) {
    // If we're resuming layout after a fragmentainer break, we need to skip
    // siblings that we're done with. We may have been able to fully lay out
    // some node(s) preceding a node that we had to break inside (and therefore
    // were not able to fully lay out). This happens when we have parallel
    // flows [1], which are caused by floats, overflow, etc.
    //
    // [1] https://drafts.csswg.org/css-break/#parallel-flows
    const auto& child_break_tokens = break_token_->ChildBreakTokens();

    if (resuming_at_inline_formatting_context_) {
      // When resuming inside an inline formatting context, just process the
      // break tokens. There'll be any number of break tokens for broken floats,
      // followed by at most one break token for the actual inline formatting
      // context where we had to give up in the previous fragmentainer. The node
      // structure will not be of any help at all, since the break tokens will
      // be associated with nodes that are not siblings.
      while (child_token_idx_ < child_break_tokens.size()) {
        const auto* token = child_break_tokens[child_token_idx_];
        child_token_idx_++;
        if (!token->IsFinished())
          return Entry(token->InputNode(), token);
      }
      return Entry(nullptr, nullptr);
    }

    do {
      // Early exit if we've exhausted our child break tokens.
      if (child_token_idx_ >= child_break_tokens.size())
        break;

      // This child break token candidate doesn't match the current node, this
      // node must be unfinished.
      const NGBreakToken* child_break_token_candidate =
          child_break_tokens[child_token_idx_];
      if (child_break_token_candidate->InputNode() != child_)
        break;

      ++child_token_idx_;

      // We have only found a node if its break token is unfinished.
      if (!child_break_token_candidate->IsFinished()) {
        child_break_token = child_break_token_candidate;
        break;
      }
    } while ((child_ = child_.NextSibling()));
  }

  NGLayoutInputNode child = child_;
  if (child_)
    child_ = child_.NextSibling();

  return Entry(child, child_break_token);
}

}  // namespace blink
