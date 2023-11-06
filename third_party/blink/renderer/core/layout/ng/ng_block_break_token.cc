// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"

#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsNGBlockBreakToken : NGBreakToken {
  Member<LayoutBox> data;
  unsigned numbers[1];
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

NGBlockBreakToken* NGBlockBreakToken::CreateRepeated(const NGBlockNode& node,
                                                     unsigned sequence_number) {
  auto* token = MakeGarbageCollected<NGBlockBreakToken>(PassKey(), node);
  token->data_ = MakeGarbageCollected<NGBlockBreakTokenData>();
  token->data_->sequence_number = sequence_number;
  token->is_repeated_ = true;
  return token;
}

NGBlockBreakToken* NGBlockBreakToken::CreateForBreakInRepeatedFragment(
    const NGBlockNode& node,
    unsigned sequence_number,
    LayoutUnit consumed_block_size,
    bool is_at_block_end) {
  auto* token = MakeGarbageCollected<NGBlockBreakToken>(PassKey(), node);
  token->data_ = MakeGarbageCollected<NGBlockBreakTokenData>();
  token->data_->sequence_number = sequence_number;
  token->data_->consumed_block_size = consumed_block_size;
  token->is_at_block_end_ = is_at_block_end;
#if DCHECK_IS_ON()
  token->is_repeated_actual_break_ = true;
#endif
  return token;
}

NGBlockBreakToken::NGBlockBreakToken(PassKey key, NGBoxFragmentBuilder* builder)
    : NGBreakToken(kBlockBreakToken, builder->node_),
      const_num_children_(builder->child_break_tokens_.size()) {
  has_seen_all_children_ = builder->has_seen_all_children_;
  is_caused_by_column_spanner_ = builder->FoundColumnSpanner();
  is_at_block_end_ = builder->is_at_block_end_;
  has_unpositioned_list_marker_ =
      static_cast<bool>(builder->GetUnpositionedListMarker());
  DCHECK(builder->HasBreakTokenData());
  data_ = builder->break_token_data_;
  builder->break_token_data_ = nullptr;
  for (wtf_size_t i = 0; i < builder->child_break_tokens_.size(); ++i)
    child_break_tokens_[i] = builder->child_break_tokens_[i];
}

NGBlockBreakToken::NGBlockBreakToken(PassKey key, NGLayoutInputNode node)
    : NGBreakToken(kBlockBreakToken, node),
      data_(MakeGarbageCollected<NGBlockBreakTokenData>()),
      const_num_children_(0) {}

const InlineBreakToken* NGBlockBreakToken::InlineBreakTokenFor(
    const NGLayoutInputNode& node) const {
  DCHECK(node.GetLayoutBox());
  return InlineBreakTokenFor(*node.GetLayoutBox());
}

const InlineBreakToken* NGBlockBreakToken::InlineBreakTokenFor(
    const LayoutBox& layout_object) const {
  DCHECK(&layout_object);
  for (const NGBreakToken* child : ChildBreakTokens()) {
    switch (child->Type()) {
      case kBlockBreakToken:
        // Currently there are no cases where InlineBreakToken is stored in
        // non-direct child descendants.
        DCHECK(
            !To<NGBlockBreakToken>(child)->InlineBreakTokenFor(layout_object));
        break;
      case kInlineBreakToken:
        if (child->InputNode().GetLayoutBox() == &layout_object)
          return To<InlineBreakToken>(child);
        break;
    }
  }
  return nullptr;
}

#if DCHECK_IS_ON()

String NGBlockBreakToken::ToString() const {
  StringBuilder string_builder;
  string_builder.Append(InputNode().ToString());
  if (is_break_before_) {
    if (is_forced_break_) {
      string_builder.Append(" forced");
    }
    string_builder.Append(" break-before");
  } else {
    string_builder.Append(" sequence:");
    string_builder.AppendNumber(SequenceNumber());
  }
  if (is_repeated_)
    string_builder.Append(" (repeated)");
  if (is_caused_by_column_spanner_) {
    string_builder.Append(" (caused by spanner)");
  }
  if (has_seen_all_children_) {
    string_builder.Append(" (seen all children)");
  }
  if (is_at_block_end_) {
    string_builder.Append(" (at block-end)");
  }
  string_builder.Append(" consumed:");
  string_builder.Append(ConsumedBlockSize().ToString());
  string_builder.Append("px");

  if (ConsumedBlockSizeForLegacy() != ConsumedBlockSize()) {
    string_builder.Append(" legacy consumed:");
    string_builder.Append(ConsumedBlockSizeForLegacy().ToString());
    string_builder.Append("px");
  }

  if (MonolithicOverflow()) {
    string_builder.Append(" monolithic overflow:");
    string_builder.Append(MonolithicOverflow().ToString());
    string_builder.Append("px");
  }

  return string_builder.ToString();
}

#endif  // DCHECK_IS_ON()

void NGBlockBreakToken::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(data_);
  // Looking up |ChildBreakTokensInternal()| in Trace() here is safe because
  // |const_num_children_| is const.
  for (auto& child : ChildBreakTokensInternal())
    visitor->Trace(child);
  NGBreakToken::TraceAfterDispatch(visitor);
}

}  // namespace blink
