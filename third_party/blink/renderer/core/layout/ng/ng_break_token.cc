// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct SameSizeAsNGBreakToken : RefCounted<NGBreakToken> {
  virtual ~SameSizeAsNGBreakToken() = default;
  void* pointer;
  unsigned flags;
};

static_assert(sizeof(NGBreakToken) == sizeof(SameSizeAsNGBreakToken),
              "NGBreakToken should stay small");

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
    for (const auto* child : children)
      AppendBreakTokenToString(child, string_builder, indent + 2);
  }
}
}  // namespace

String NGBreakToken::ToString() const {
  StringBuilder string_builder;
  string_builder.Append("(");
  string_builder.Append(InputNode().ToString());
  string_builder.Append(")");
  if (IsFinished())
    string_builder.Append(" finished");
  return string_builder.ToString();
}

void NGBreakToken::ShowBreakTokenTree() const {
  StringBuilder string_builder;
  string_builder.Append(".:: LayoutNG Break Token Tree ::.\n");
  AppendBreakTokenToString(this, &string_builder);
  fprintf(stderr, "%s\n", string_builder.ToString().Utf8().c_str());
}
#endif  // DCHECK_IS_ON()

}  // namespace blink
