// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsNGBreakToken : GarbageCollected<NGBreakToken> {
  Member<void*> member;
  unsigned flags;
};

ASSERT_SIZE(NGBreakToken, SameSizeAsNGBreakToken);

}  // namespace

#if DCHECK_IS_ON()

namespace {

void AppendBreakTokenToString(const NGBreakToken* token,
                              StringBuilder* string_builder,
                              unsigned indent = 2) {
  if (!token)
    return;
  DCHECK(string_builder);

  for (unsigned i = 0; i < indent; i++)
    string_builder->Append(" ");
  string_builder->Append(token->ToString());
  string_builder->Append("\n");

  if (auto* block_break_token = DynamicTo<NGBlockBreakToken>(token)) {
    const auto children = block_break_token->ChildBreakTokens();
    for (const auto& child : children)
      AppendBreakTokenToString(child, string_builder, indent + 2);
  }
}
}  // namespace

String NGBreakToken::ToString() const {
  switch (Type()) {
    case kBlockBreakToken:
      return To<NGBlockBreakToken>(this)->ToString();
    case kInlineBreakToken:
      return To<NGInlineBreakToken>(this)->ToString();
  }
  NOTREACHED();
}

void NGBreakToken::ShowBreakTokenTree() const {
  StringBuilder string_builder;
  string_builder.Append(".:: LayoutNG Break Token Tree ::.\n");
  AppendBreakTokenToString(this, &string_builder);
  fprintf(stderr, "%s\n", string_builder.ToString().Utf8().c_str());
}
#endif  // DCHECK_IS_ON()

void NGBreakToken::Trace(Visitor* visitor) const {
  switch (Type()) {
    case kBlockBreakToken:
      To<NGBlockBreakToken>(this)->TraceAfterDispatch(visitor);
      return;
    case kInlineBreakToken:
      To<NGInlineBreakToken>(this)->TraceAfterDispatch(visitor);
      return;
  }
  NOTREACHED();
}

void NGBreakToken::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(box_);
}

}  // namespace blink
