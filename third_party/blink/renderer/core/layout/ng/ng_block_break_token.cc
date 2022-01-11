// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsNGBlockBreakToken : NGBreakToken {
  LayoutUnit block_sizes[2];
  std::unique_ptr<void> flex_data;
  std::unique_ptr<void> grid_data;
  unsigned numbers[2];
};

ASSERT_SIZE(NGBlockBreakToken, SameSizeAsNGBlockBreakToken);

}  // namespace

NGBlockBreakToken* NGBlockBreakToken::Create(NGBoxFragmentBuilder* builder) {
  // We store the children list inline in the break token as a flexible
  // array. Therefore, we need to make sure to allocate enough space for that
  // array here, which requires a manual allocation + placement new.
  return MakeGarbageCollected<NGBlockBreakToken>(
      AdditionalBytes(builder->child_break_tokens_.size() *
                      sizeof(Member<NGBreakToken>)),
      PassKey(), builder);
}

NGBlockBreakToken::NGBlockBreakToken(PassKey key, NGBoxFragmentBuilder* builder)
    : NGBreakToken(kBlockBreakToken, builder->node_),
      consumed_block_size_(builder->consumed_block_size_),
      consumed_block_size_legacy_adjustment_(
          builder->consumed_block_size_legacy_adjustment_),
      sequence_number_(builder->sequence_number_),
      const_num_children_(builder->child_break_tokens_.size()) {
  has_seen_all_children_ = builder->has_seen_all_children_;
  is_caused_by_column_spanner_ = builder->FoundColumnSpanner();
  is_at_block_end_ = builder->is_at_block_end_;
  has_unpositioned_list_marker_ =
      static_cast<bool>(builder->UnpositionedListMarker());
  if (builder->flex_break_token_data_)
    flex_data_ = std::move(builder->flex_break_token_data_);
  if (builder->grid_break_token_data_)
    grid_data_ = std::move(builder->grid_break_token_data_);
  for (wtf_size_t i = 0; i < builder->child_break_tokens_.size(); ++i)
    child_break_tokens_[i] = builder->child_break_tokens_[i];
}

NGBlockBreakToken::NGBlockBreakToken(PassKey key, NGLayoutInputNode node)
    : NGBreakToken(kBlockBreakToken, node), const_num_children_(0) {}

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

  if (consumed_block_size_legacy_adjustment_) {
    string_builder.Append(" legacy adjustment:");
    string_builder.Append(consumed_block_size_legacy_adjustment_.ToString());
    string_builder.Append("px");
  }

  return string_builder.ToString();
}

#endif  // DCHECK_IS_ON()

void NGBlockBreakToken::Trace(Visitor* visitor) const {
  // Looking up |ChildBreakTokens()| in Trace() here is safe because
  // |const_num_children_| is const.
  for (auto& child : ChildBreakTokens())
    visitor->Trace(child);
  NGBreakToken::Trace(visitor);
}

}  // namespace blink
