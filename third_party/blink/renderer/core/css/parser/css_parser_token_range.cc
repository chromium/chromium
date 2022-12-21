// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"

#include "third_party/blink/renderer/platform/wtf/static_constructors.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

DEFINE_GLOBAL(CSSParserToken, g_static_eof_token);

void CSSParserTokenRange::InitStaticEOFToken() {
  new ((void*)&g_static_eof_token) CSSParserToken(kEOFToken);
}

CSSParserTokenRange CSSParserTokenRange::MakeSubRange(
    const CSSParserToken* first,
    const CSSParserToken* last) const {
  if (first == &g_static_eof_token) {
    first = last_;
  }
  if (last == &g_static_eof_token) {
    last = last_;
  }
  DCHECK_LE(first, last);
  return CSSParserTokenRange(first, last);
}

CSSParserTokenRange CSSParserTokenRange::ConsumeBlock() {
  DCHECK_EQ(Peek().GetBlockType(), CSSParserToken::kBlockStart);
  const CSSParserToken* start = &Peek() + 1;
  unsigned nesting_level = 0;
  do {
    const CSSParserToken& token = Consume();
    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      nesting_level++;
    } else if (token.GetBlockType() == CSSParserToken::kBlockEnd) {
      nesting_level--;
    }
  } while (nesting_level && first_ < last_);

  if (nesting_level) {
    return MakeSubRange(start, first_);  // Ended at EOF
  }
  return MakeSubRange(start, first_ - 1);
}

void CSSParserTokenRange::ConsumeComponentValue() {
  // FIXME: This is going to do multiple passes over large sections of a
  // stylesheet. We should consider optimising this by precomputing where each
  // block ends.
  unsigned nesting_level = 0;
  do {
    const CSSParserToken& token = Consume();
    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      nesting_level++;
    } else if (token.GetBlockType() == CSSParserToken::kBlockEnd) {
      nesting_level--;
    }
  } while (nesting_level && first_ < last_);
}

String CSSParserTokenRange::Serialize() const {
  // We're supposed to insert comments between certain pairs of token types
  // as per spec, but since this is currently only used for @supports CSSOM
  // and CSS Paint API arguments we just get these cases wrong and avoid the
  // additional complexity.
  StringBuilder builder;
  for (const CSSParserToken* it = first_; it != last_; ++it) {
    it->Serialize(builder);
  }
  return builder.ReleaseString();
}

}  // namespace blink
