// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsNGBlockBreakToken : NGBreakToken {
  unsigned numbers[3];
};

ASSERT_SIZE(NGBlockBreakToken, SameSizeAsNGBlockBreakToken);

}  // namespace

scoped_refptr<NGBlockBreakToken> NGBlockBreakToken::Create(
    const NGBoxFragmentBuilder& builder) {
  // We store the children list inline in the break token as a flexible
  // array. Therefore, we need to make sure to allocate enough space for that
  // array here, which requires a manual allocation + placement new.
  void* data = ::WTF::Partitions::FastMalloc(
      sizeof(NGBlockBreakToken) +
          builder.child_break_tokens_.size() * sizeof(NGBreakToken*),
      ::WTF::GetStringWithTypeName<NGBlockBreakToken>());
  new (data) NGBlockBreakToken(PassKey(), builder);
  return base::AdoptRef(static_cast<NGBlockBreakToken*>(data));
}

NGBlockBreakToken::NGBlockBreakToken(PassKey key,
                                     const NGBoxFragmentBuilder& builder)
    : NGBreakToken(kBlockBreakToken, kUnfinished, builder.node_),
      consumed_block_size_(builder.consumed_block_size_),
      sequence_number_(builder.sequence_number_),
      num_children_(builder.child_break_tokens_.size()) {
  break_appeal_ = builder.break_appeal_;
  has_seen_all_children_ = builder.has_seen_all_children_;
  is_at_block_end_ = builder.is_at_block_end_;
  for (wtf_size_t i = 0; i < builder.child_break_tokens_.size(); ++i) {
    child_break_tokens_[i] = builder.child_break_tokens_[i].get();
    child_break_tokens_[i]->AddRef();
  }
}

NGBlockBreakToken::NGBlockBreakToken(PassKey key, NGLayoutInputNode node)
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
