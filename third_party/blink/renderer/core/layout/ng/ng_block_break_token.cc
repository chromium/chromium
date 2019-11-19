// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsNGBlockBreakToken : NGBreakToken {
  unsigned numbers[2];
};

static_assert(sizeof(NGBlockBreakToken) == sizeof(SameSizeAsNGBlockBreakToken),
              "NGBlockBreakToken should stay small");

}  // namespace

NGBlockBreakToken::NGBlockBreakToken(
    NGLayoutInputNode node,
    LayoutUnit consumed_block_size,
    const NGBreakTokenVector& child_break_tokens,
    NGBreakAppeal break_appeal,
    bool has_seen_all_children)
    : NGBreakToken(kBlockBreakToken, kUnfinished, node),
      consumed_block_size_(consumed_block_size),
      num_children_(child_break_tokens.size()) {
  break_appeal_ = break_appeal;
  has_seen_all_children_ = has_seen_all_children;
  for (wtf_size_t i = 0; i < child_break_tokens.size(); ++i) {
    child_break_tokens_[i] = child_break_tokens[i].get();
    child_break_tokens_[i]->AddRef();
  }
}

NGBlockBreakToken::NGBlockBreakToken(NGLayoutInputNode node)
    : NGBreakToken(kBlockBreakToken, kUnfinished, node), num_children_(0) {}

const NGInlineBreakToken* NGBlockBreakToken::InlineBreakTokenFor(
    const NGLayoutInputNode& node) const {
  DCHECK(node.GetLayoutBox());
  return InlineBreakTokenFor(*node.GetLayoutBox());
}

const NGInlineBreakToken* NGBlockBreakToken::InlineBreakTokenFor(
    const LayoutBox& layout_object) const {
  DCHECK(&layout_object);
  for (const NGBreakToken* child : ChildBreakTokens()) {
    switch (child->Type()) {
      case kBlockBreakToken:
        // Currently there are no cases where NGInlineBreakToken is stored in
        // non-direct child descendants.
        DCHECK(
            !To<NGBlockBreakToken>(child)->InlineBreakTokenFor(layout_object));
        break;
      case kInlineBreakToken:
        if (child->InputNode().GetLayoutBox() == &layout_object)
          return To<NGInlineBreakToken>(child);
        break;
    }
  }
  return nullptr;
}

#if DCHECK_IS_ON()

String NGBlockBreakToken::ToString() const {
  StringBuilder string_builder;
  string_builder.Append(NGBreakToken::ToString());
  string_builder.Append(" consumed:");
  string_builder.Append(consumed_block_size_.ToString());
  string_builder.Append("px");
  return string_builder.ToString();
}

#endif  // DCHECK_IS_ON()

}  // namespace blink
