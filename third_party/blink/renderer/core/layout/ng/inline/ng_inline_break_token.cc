// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsNGInlineBreakToken : NGBreakToken {
  scoped_refptr<const ComputedStyle> style_;
  unsigned numbers[2];
};

static_assert(sizeof(NGInlineBreakToken) ==
                  sizeof(SameSizeAsNGInlineBreakToken),
              "NGInlineBreakToken should stay small");

}  // namespace

NGInlineBreakToken::NGInlineBreakToken(
    NGInlineNode node,
    const ComputedStyle* style,
    unsigned item_index,
    unsigned text_offset,
    unsigned flags /* NGInlineBreakTokenFlags */)
    : NGBreakToken(kInlineBreakToken, kUnfinished, node),
      style_(style),
      item_index_(item_index),
      text_offset_(text_offset) {
  flags_ = flags;
}

NGInlineBreakToken::NGInlineBreakToken(NGLayoutInputNode node)
    : NGBreakToken(kInlineBreakToken, kFinished, node),
      item_index_(0),
      text_offset_(0) {}

NGInlineBreakToken::~NGInlineBreakToken() = default;

#if DCHECK_IS_ON()

String NGInlineBreakToken::ToString() const {
  StringBuilder string_builder;
  string_builder.Append(NGBreakToken::ToString());
  if (!IsFinished()) {
    string_builder.Append(
        String::Format(" index:%u offset:%u", ItemIndex(), TextOffset()));
    if (IsForcedBreak())
      string_builder.Append(" forced");
  }
  return string_builder.ToString();
}

#endif  // DCHECK_IS_ON()

}  // namespace blink
