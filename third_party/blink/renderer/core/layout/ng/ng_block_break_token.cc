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
  std::unique_ptr<void> data;
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

NGBlockBreakToken::NGBlockBreakToken(PassKey key, NGBoxFragmentBuilder* builder)
    : NGBreakToken(kBlockBreakToken, builder->node_),
      const_num_children_(builder->child_break_tokens_.size()) {
  has_seen_all_children_ = builder->has_seen_all_children_;
  is_caused_by_column_spanner_ = builder->FoundColumnSpanner();
  is_at_block_end_ = builder->is_at_block_end_;
  has_unpositioned_list_marker_ =
      static_cast<bool>(builder->UnpositionedListMarker());
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
  string_builder.Append(ConsumedBlockSize().ToString());
  string_builder.Append("px");

  if (ConsumedBlockSizeForLegacy()) {
    string_builder.Append(" legacy adjustment:");
    string_builder.Append(ConsumedBlockSizeForLegacy().ToString());
    string_builder.Append("px");
  }

  return string_builder.ToString();
}

#endif  // DCHECK_IS_ON()

void NGBlockBreakToken::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(data_);
  // Looking up |ChildBreakTokens()| in Trace() here is safe because
  // |const_num_children_| is const.
  for (auto& child : ChildBreakTokens())
    visitor->Trace(child);
  NGBreakToken::TraceAfterDispatch(visitor);
}

}  // namespace blink
