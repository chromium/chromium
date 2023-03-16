// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsNGInlineBreakToken : NGBreakToken {
  scoped_refptr<const ComputedStyle> style_;
  unsigned numbers[2];
};

ASSERT_SIZE(NGInlineBreakToken, SameSizeAsNGInlineBreakToken);

}  // namespace

const Member<const NGBreakToken>* NGInlineBreakToken::SubBreakTokenAddress()
    const {
  CHECK(flags_ & kHasSubBreakToken);
  return sub_break_token_;
}

const NGBlockBreakToken* NGInlineBreakToken::BlockInInlineBreakToken() const {
  if (!(flags_ & kHasSubBreakToken))
    return nullptr;
  const Member<const NGBreakToken>* ptr = SubBreakTokenAddress();
  DCHECK(*ptr);
  if ((*ptr)->IsBlockType())
    return To<NGBlockBreakToken>(ptr->Get());
  return nullptr;
}

const NGInlineBreakToken* NGInlineBreakToken::SubBreakTokenInParallelFlow()
    const {
  if (!(flags_ & kHasSubBreakToken))
    return nullptr;
  const Member<const NGBreakToken>* ptr = SubBreakTokenAddress();
  DCHECK(*ptr);
  if ((*ptr)->IsInlineType())
    return To<NGInlineBreakToken>(ptr->Get());
  return nullptr;
}

// static
NGInlineBreakToken* NGInlineBreakToken::Create(
    NGInlineNode node,
    const ComputedStyle* style,
    const NGInlineItemTextIndex& index,
    unsigned flags /* NGInlineBreakTokenFlags */,
    const NGBreakToken* sub_break_token) {
  // We store the children list inline in the break token as a flexible
  // array. Therefore, we need to make sure to allocate enough space for that
  // array here, which requires a manual allocation + placement new.
  wtf_size_t size = sizeof(NGInlineBreakToken);
  if (UNLIKELY(sub_break_token)) {
    if (sub_break_token->IsInlineType())
      size += sizeof(Member<const NGInlineBreakToken>);
    else
      size += sizeof(Member<const NGBlockBreakToken>);
    flags |= kHasSubBreakToken;
  }

  return MakeGarbageCollected<NGInlineBreakToken>(AdditionalBytes(size),
                                                  PassKey(), node, style, index,
                                                  flags, sub_break_token);
}

NGInlineBreakToken::NGInlineBreakToken(
    PassKey key,
    NGInlineNode node,
    const ComputedStyle* style,
    const NGInlineItemTextIndex& index,
    unsigned flags /* NGInlineBreakTokenFlags */,
    const NGBreakToken* sub_break_token)
    : NGBreakToken(kInlineBreakToken, node, flags),
      style_(style),
      index_(index) {
  if (UNLIKELY(sub_break_token)) {
#if DCHECK_IS_ON()
    // Only one level of inline break token nesting is expected.
    DCHECK(!sub_break_token->IsInlineType() ||
           To<NGInlineBreakToken>(sub_break_token)->BlockInInlineBreakToken());
#endif
    const Member<const NGBreakToken>* ptr = SubBreakTokenAddress();
    *const_cast<Member<const NGBreakToken>*>(ptr) = sub_break_token;
  }
}

bool NGInlineBreakToken::IsAfterBlockInInline() const {
  if (!ItemIndex())
    return false;
  const auto node = To<NGInlineNode>(InputNode());
  const NGInlineItemsData& items_data = node.ItemsData(/*is_first_line*/ false);
  const NGInlineItem& last_item = items_data.items[ItemIndex() - 1];
  return last_item.Type() == NGInlineItem::kBlockInInline &&
         TextOffset() == last_item.EndOffset();
}

#if DCHECK_IS_ON()

String NGInlineBreakToken::ToString() const {
  StringBuilder string_builder;
  string_builder.Append(
      String::Format(" index:%u offset:%u", ItemIndex(), TextOffset()));
  if (IsForcedBreak())
    string_builder.Append(" forced");
  return string_builder.ToString();
}

#endif  // DCHECK_IS_ON()

void NGInlineBreakToken::TraceAfterDispatch(Visitor* visitor) const {
  // It is safe to check flags_ here because it is a const value and initialized
  // in ctor.
  if (flags_ & kHasSubBreakToken)
    visitor->Trace(*sub_break_token_);
  NGBreakToken::TraceAfterDispatch(visitor);
}

}  // namespace blink
