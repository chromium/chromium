// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockChildIterator_h
#define NGBlockChildIterator_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

namespace blink {

class NGBreakToken;
class NGBlockBreakToken;

// A utility class for block-flow layout which given the first child and a
// break token will iterate through unfinished children.
//
// This class does not handle modifications to its arguments after it has been
// constructed.
class CORE_EXPORT NGBlockChildIterator {
  STACK_ALLOCATED();

 public:
  NGBlockChildIterator(NGLayoutInputNode first_child,
                       const NGBlockBreakToken* break_token);

  // Returns the next input node which should be laid out, along with its
  // respective break token.
  // @param previous_inline_break_token The previous inline break token is
  //    needed as multiple line-boxes can exist within the same parent
  //    fragment, unlike blocks.
  struct Entry;
  Entry NextChild(const NGBreakToken* previous_inline_break_token = nullptr);

 private:
  NGLayoutInputNode child_;
  const NGBlockBreakToken* break_token_;

  // An index into break_token_'s ChildBreakTokens() vector. Used for keeping
  // track of the next child break token to inspect.
  wtf_size_t child_token_idx_;

  bool resuming_at_inline_formatting_context_ = false;
};

struct NGBlockChildIterator::Entry {
  STACK_ALLOCATED();

 public:
  Entry(NGLayoutInputNode node, const NGBreakToken* token)
      : node(node), token(token) {}

  NGLayoutInputNode node;
  const NGBreakToken* token;

  bool operator==(const NGBlockChildIterator::Entry& other) const {
    return node == other.node && token == other.token;
  }
};

}  // namespace blink

#endif  // NGBlockChildIterator_h
