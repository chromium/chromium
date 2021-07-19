// Copyright 2017 The Chromium Authors. All rights reserved.
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

const NGBlockBreakToken* const*
NGInlineBreakToken::BlockInInlineBreakTokenAddress() const {
  CHECK(flags_ & kHasBlockInInlineToken);
  return block_in_inline_break_token_;
}

const NGBlockBreakToken* NGInlineBreakToken::BlockInInlineBreakToken() const {
  if (!(flags_ & kHasBlockInInlineToken))
    return nullptr;
  const NGBlockBreakToken* const* ptr = BlockInInlineBreakTokenAddress();
  DCHECK(*ptr);
  return *ptr;
}

// static
scoped_refptr<NGInlineBreakToken> NGInlineBreakToken::Create(
    NGInlineNode node,
    const ComputedStyle* style,
    unsigned item_index,
    unsigned text_offset,
    unsigned flags /* NGInlineBreakTokenFlags */,
    const NGBlockBreakToken* block_in_inline_break_token) {
  // We store the children list inline in the break token as a flexible
  // array. Therefore, we need to make sure to allocate enough space for that
  // array here, which requires a manual allocation + placement new.
  wtf_size_t size = sizeof(NGInlineBreakToken);
  if (UNLIKELY(block_in_inline_break_token)) {
    size += sizeof(NGBlockBreakToken*);
    flags |= kHasBlockInInlineToken;
  }

  void* data = ::WTF::Partitions::FastMalloc(
      size, ::WTF::GetStringWithTypeName<NGInlineBreakToken>());
  new (data) NGInlineBreakToken(PassKey(), node, style, item_index, text_offset,
                                flags, block_in_inline_break_token);
  return base::AdoptRef(static_cast<NGInlineBreakToken*>(data));
}

NGInlineBreakToken::NGInlineBreakToken(
    PassKey key,
    NGInlineNode node,
    const ComputedStyle* style,
    unsigned item_index,
    unsigned text_offset,
    unsigned flags /* NGInlineBreakTokenFlags */,
    const NGBlockBreakToken* block_in_inline_break_token)
    : NGBreakToken(kInlineBreakToken, node),
      style_(style),
      item_index_(item_index),
      text_offset_(text_offset) {
  flags_ = flags;

  if (UNLIKELY(block_in_inline_break_token)) {
    block_in_inline_break_token->AddRef();
    const NGBlockBreakToken* const* ptr = BlockInInlineBreakTokenAddress();
    *const_cast<const NGBlockBreakToken**>(ptr) = block_in_inline_break_token;
  }
}

NGInlineBreakToken::~NGInlineBreakToken() {
  if (UNLIKELY(flags_ & kHasBlockInInlineToken)) {
    const NGBlockBreakToken* const* ptr = BlockInInlineBreakTokenAddress();
    DCHECK(*ptr);
    (*ptr)->Release();
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
  string_builder.Append(NGBreakToken::ToString());
  string_builder.Append(
      String::Format(" index:%u offset:%u", ItemIndex(), TextOffset()));
  if (IsForcedBreak())
    string_builder.Append(" forced");
  return string_builder.ToString();
}

#endif  // DCHECK_IS_ON()

}  // namespace blink
