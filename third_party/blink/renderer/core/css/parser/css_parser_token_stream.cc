// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"

namespace blink {

StringView CSSParserTokenStream::StringRangeAt(wtf_size_t start,
                                               wtf_size_t length) const {
  return tokenizer_.StringRangeAt(start, length);
}

void CSSParserTokenStream::ConsumeWhitespace() {
  while (Peek().GetType() == kWhitespaceToken)
    UncheckedConsume();
}

CSSParserToken CSSParserTokenStream::ConsumeIncludingWhitespace() {
  CSSParserToken result = Consume();
  ConsumeWhitespace();
  return result;
}

bool CSSParserTokenStream::ConsumeCommentOrNothing() {
  DCHECK(!HasLookAhead());
  const auto token = tokenizer_.TokenizeSingleWithComments();
  if (token.GetType() != kCommentToken) {
    next_ = token;
    has_look_ahead_ = true;
    return false;
  }

  has_look_ahead_ = false;
  offset_ = tokenizer_.Offset();
  return true;
}

void CSSParserTokenStream::UncheckedConsumeComponentValue() {
  DCHECK(HasLookAhead());

  // Have to use internal consume/peek in here because they can read past
  // start/end of blocks
  unsigned nesting_level = 0;
  do {
    const CSSParserToken& token = UncheckedConsumeInternal();
    if (token.GetBlockType() == CSSParserToken::kBlockStart)
      nesting_level++;
    else if (token.GetBlockType() == CSSParserToken::kBlockEnd)
      nesting_level--;
  } while (!PeekInternal().IsEOF() && nesting_level);
}

void CSSParserTokenStream::UncheckedSkipToEndOfBlock() {
  DCHECK(HasLookAhead());
  // Have to use internal consume/peek in here because they can read past
  // start/end of blocks
  unsigned nesting_level = 1;
  do {
    const CSSParserToken& token = UncheckedConsumeInternal();
    if (token.GetBlockType() == CSSParserToken::kBlockStart)
      nesting_level++;
    else if (token.GetBlockType() == CSSParserToken::kBlockEnd)
      nesting_level--;
  } while (nesting_level && !PeekInternal().IsEOF());
}

}  // namespace blink
