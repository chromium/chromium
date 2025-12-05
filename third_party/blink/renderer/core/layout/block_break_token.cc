// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/block_break_token.h"

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/break_token_algorithm_data.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsBlockBreakToken : BreakToken {
  Member<LayoutBox> data;
  LayoutUnit consumed_block_size;
  LayoutUnit monolithic_overflow;
  LogicalOffset oof_start_offset;
  unsigned sequence_number;
  unsigned numbers[1];
};

ASSERT_SIZE(BlockBreakToken, SameSizeAsBlockBreakToken);

}  // namespace

BlockBreakToken* BlockBreakToken::Create(BoxFragmentBuilder* builder) {
  // We store the children list inline in the break token as a flexible
  // array. Therefore, we need to make sure to allocate enough space for that
  // array here, which requires a manual allocation + placement new.
  return MakeGarbageCollected<BlockBreakToken>(
      AdditionalBytes(builder->child_break_tokens_.size() *
                      sizeof(Member<BreakToken>)),
      PassKey(), builder);
}

BlockBreakToken* BlockBreakToken::CreateRepeated(const BlockNode& node,
                                                 unsigned sequence_number) {
  auto* token = MakeGarbageCollected<BlockBreakToken>(PassKey(), node);
  token->sequence_number_ = sequence_number;
  token->is_repeated_ = true;
  return token;
}

BlockBreakToken* BlockBreakToken::CreateForBreakInRepeatedFragment(
    const BlockNode& node,
    unsigned sequence_number,
    LayoutUnit consumed_block_size,
    bool is_at_block_end) {
  auto* token = MakeGarbageCollected<BlockBreakToken>(PassKey(), node);
  token->sequence_number_ = sequence_number;
  token->consumed_block_size_ = consumed_block_size;
  token->is_at_block_end_ = is_at_block_end;
  token->is_repeated_actual_break_ = true;
  return token;
}

BlockBreakToken::BlockBreakToken(PassKey key, BoxFragmentBuilder* builder)
    : BreakToken(kBlockBreakToken, builder->node_),
      const_num_children_(builder->child_break_tokens_.size()) {
  has_seen_all_children_ = builder->has_seen_all_children_;
  is_caused_by_column_spanner_ = builder->FoundColumnSpanner();
  is_at_block_end_ = builder->is_at_block_end_;
  has_unpositioned_list_marker_ =
      static_cast<bool>(builder->GetUnpositionedListMarker());
  data_ = builder->break_token_data_;
  builder->break_token_data_ = nullptr;
  consumed_block_size_ = builder->consumed_block_size_;
  monolithic_overflow_ = builder->monolithic_overflow_;
  sequence_number_ = builder->sequence_number_;

  // Place OOF break tokens first. They need to be visited before in-flow
  // breaks, since the container will stop layout if resuming at an in-flow
  // break causes another in-flow break. Note that since the OutOfFlowLayoutPart
  // step is run after in-flow child layout, the OOF fragments themselves will
  // still end up after in-flow siblings.
  if (RuntimeEnabledFeatures::FragmentedOofInCbEnabled()) {
    std::stable_sort(builder->child_break_tokens_.begin(),
                     builder->child_break_tokens_.end(),
                     [](const Member<const BreakToken>& a,
                        const Member<const BreakToken>& b) {
                       return !a->InputNode().IsOutOfFlowPositioned() <
                              !b->InputNode().IsOutOfFlowPositioned();
                     });
  }

  for (wtf_size_t i = 0; i < const_num_children_; ++i) {
    // SAFETY: `const_num_children_` ensures buffer access never goes out of
    // range.
    UNSAFE_BUFFERS(child_break_tokens_[i]) = builder->child_break_tokens_[i];
  }
}

BlockBreakToken::BlockBreakToken(PassKey key, LayoutInputNode node)
    : BreakToken(kBlockBreakToken, node),
      const_num_children_(0) {}

void BlockBreakToken::MutableForOofFragmentation::Merge(
    const BlockBreakToken& new_break_token) {
  DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  if (LayoutUnit monolithic_overflow = new_break_token.MonolithicOverflow()) {
    DCHECK_GT(monolithic_overflow, LayoutUnit());
    break_token_.monolithic_overflow_ =
        std::max(break_token_.monolithic_overflow_, monolithic_overflow);
  }
}

String BlockBreakToken::ToString(bool skip_node_info) const {
  StringBuilder string_builder;
  if (!skip_node_info) {
    string_builder.Append(InputNode().ToString());
  }
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

  if (oof_start_offset_ != LogicalOffset()) {
    string_builder.Append(" oof-offset:");
    string_builder.Append(oof_start_offset_.ToString());
  }

  string_builder.Append(" consumed:");
  string_builder.Append(ConsumedBlockSize().ToString());
  string_builder.Append("px");

  if (!is_repeated_actual_break_ && MonolithicOverflow()) {
    string_builder.Append(" monolithic overflow:");
    string_builder.Append(MonolithicOverflow().ToString());
    string_builder.Append("px");
  }

  return string_builder.ToString();
}

void BlockBreakToken::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(data_);
  // Looking up |ChildBreakTokensInternal()| in Trace() here is safe because
  // |const_num_children_| is const.
  for (wtf_size_t i = 0; i < const_num_children_; ++i) {
    // SAFETY: `const_num_children_` ensures buffer access never goes out of
    // range.
    visitor->Trace(UNSAFE_BUFFERS(child_break_tokens_[i]));
  }
  BreakToken::TraceAfterDispatch(visitor);
}

}  // namespace blink
