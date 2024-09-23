// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsInlineBreakToken : BreakToken {
  Member<const ComputedStyle> style;
  unsigned numbers[2];
};

ASSERT_SIZE(InlineBreakToken, SameSizeAsInlineBreakToken);

}  // namespace

const BlockBreakToken* InlineBreakToken::GetBlockBreakToken() const {
  if (!(flags_ & kHasRareData)) {
    return nullptr;
  }
  return rare_data_[0].sub_break_token.Get();
}

const RubyBreakTokenData* InlineBreakToken::RubyData() const {
  if (!(flags_ & kHasRareData)) {
    return nullptr;
  }
  return rare_data_[0].ruby_data.Get();
}

// static
InlineBreakToken* InlineBreakToken::Create(
    InlineNode node,
    const ComputedStyle* style,
    const InlineItemTextIndex& start,
    unsigned flags /* InlineBreakTokenFlags */,
    const BlockBreakToken* sub_break_token,
    const RubyBreakTokenData* ruby_data) {
  // We store the children list inline in the break token as a flexible
  // array. Therefore, we need to make sure to allocate enough space for that
  // array here, which requires a manual allocation + placement new.
  wtf_size_t size = sizeof(InlineBreakToken);
  if (sub_break_token || ruby_data) [[unlikely]] {
    size += sizeof(RareData);
    flags |= kHasRareData;
  }

  return MakeGarbageCollected<InlineBreakToken>(
      AdditionalBytes(size), PassKey(), node, style, start, flags,
      sub_break_token, ruby_data);
}

// static
InlineBreakToken* InlineBreakToken::CreateForParallelBlockFlow(
    InlineNode node,
    const InlineItemTextIndex& start,
    const BlockBreakToken& child_break_token) {
  return Create(node, &node.Style(), start, kIsInParallelBlockFlow,
                &child_break_token);
}

InlineBreakToken::InlineBreakToken(PassKey key,
                                   InlineNode node,
                                   const ComputedStyle* style,
                                   const InlineItemTextIndex& start,
                                   unsigned flags /* InlineBreakTokenFlags */,
                                   const BlockBreakToken* sub_break_token,
                                   const RubyBreakTokenData* ruby_data)
    : BreakToken(kInlineBreakToken, node, flags), style_(style), start_(start) {
  if (sub_break_token || ruby_data) [[unlikely]] {
    rare_data_[0].sub_break_token = sub_break_token;
    rare_data_[0].ruby_data = ruby_data;
  }
}

#if DCHECK_IS_ON()

String InlineBreakToken::ToString() const {
  StringBuilder string_builder;
  string_builder.Append(String::Format("InlineBreakToken index:%u offset:%u",
                                       StartItemIndex(), StartTextOffset()));
  if (UseFirstLineStyle()) {
    string_builder.Append(" first-line");
  }
  if (IsForcedBreak())
    string_builder.Append(" forced");
  if (HasClonedBoxDecorations()) {
    string_builder.Append(" cloned-box-decorations");
  }
  if (IsInParallelBlockFlow()) {
    string_builder.Append(" parallel-flow");
  }
  return string_builder.ToString();
}

#endif  // DCHECK_IS_ON()

void InlineBreakToken::TraceAfterDispatch(Visitor* visitor) const {
  // It is safe to check flags_ here because it is a const value and initialized
  // in ctor.
  if (flags_ & kHasRareData) {
    visitor->Trace(rare_data_[0]);
  }
  visitor->Trace(style_);
  BreakToken::TraceAfterDispatch(visitor);
}

void InlineBreakToken::RareData::Trace(Visitor* visitor) const {
  visitor->Trace(sub_break_token);
  visitor->Trace(ruby_data);
}

}  // namespace blink
