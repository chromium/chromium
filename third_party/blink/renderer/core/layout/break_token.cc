// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/break_token.h"

#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsBreakToken : GarbageCollected<BreakToken> {
  Member<void*> member;
  unsigned flags;
};

ASSERT_SIZE(BreakToken, SameSizeAsBreakToken);

}  // namespace

bool BreakToken::IsInParallelFlow() const {
  if (const auto* block_break_token = DynamicTo<BlockBreakToken>(this)) {
    return block_break_token->IsAtBlockEnd();
  }
  if (const auto* inline_break_token = DynamicTo<InlineBreakToken>(this)) {
    return inline_break_token->IsInParallelBlockFlow();
  }
  return false;
}

#if DCHECK_IS_ON()

namespace {

void AppendBreakTokenToString(const BreakToken* token,
                              StringBuilder* string_builder,
                              unsigned indent = 2) {
  if (!token)
    return;
  DCHECK(string_builder);

  for (unsigned i = 0; i < indent; i++)
    string_builder->Append(" ");
  string_builder->Append(token->ToString());
  string_builder->Append("\n");

  if (auto* block_break_token = DynamicTo<BlockBreakToken>(token)) {
    const auto children = block_break_token->ChildBreakTokens();
    for (const auto& child : children)
      AppendBreakTokenToString(child, string_builder, indent + 2);
  } else if (auto* inline_break_token = DynamicTo<InlineBreakToken>(token)) {
    if (auto* child_block_break_token =
            inline_break_token->GetBlockBreakToken()) {
      AppendBreakTokenToString(child_block_break_token, string_builder,
                               indent + 2);
    }
  }
}
}  // namespace

String BreakToken::ToString() const {
  switch (Type()) {
    case kBlockBreakToken:
      return To<BlockBreakToken>(this)->ToString();
    case kInlineBreakToken:
      return To<InlineBreakToken>(this)->ToString();
  }
  NOTREACHED_IN_MIGRATION();
}

void BreakToken::ShowBreakTokenTree() const {
  StringBuilder string_builder;
  string_builder.Append(".:: LayoutNG Break Token Tree ::.\n");
  AppendBreakTokenToString(this, &string_builder);
  fprintf(stderr, "%s\n", string_builder.ToString().Utf8().c_str());
}
#endif  // DCHECK_IS_ON()

void BreakToken::Trace(Visitor* visitor) const {
  switch (Type()) {
    case kBlockBreakToken:
      To<BlockBreakToken>(this)->TraceAfterDispatch(visitor);
      return;
    case kInlineBreakToken:
      To<InlineBreakToken>(this)->TraceAfterDispatch(visitor);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void BreakToken::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(box_);
}

}  // namespace blink
