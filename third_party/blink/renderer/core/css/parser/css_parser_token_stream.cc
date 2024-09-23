// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"

namespace blink {

StringView CSSParserTokenStream::StringRangeAt(wtf_size_t start,
                                               wtf_size_t length) const {
  return tokenizer_.StringRangeAt(start, length);
}

StringView CSSParserTokenStream::RemainingText() const {
  wtf_size_t start = HasLookAhead() ? LookAheadOffset() : Offset();
  return tokenizer_.StringRangeFrom(start);
}

void CSSParserTokenStream::ConsumeWhitespace() {
  while (Peek().GetType() == kWhitespaceToken) {
    UncheckedConsume();
  }
}

CSSParserToken CSSParserTokenStream::ConsumeIncludingWhitespace() {
  CSSParserToken result = Consume();
  ConsumeWhitespace();
  return result;
}

CSSParserToken CSSParserTokenStream::ConsumeIncludingWhitespaceRaw() {
  CSSParserToken result = ConsumeRaw();
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

void CSSParserTokenStream::UncheckedSkipToEndOfBlock() {
  DCHECK(HasLookAhead());

  // Process and consume the lookahead token.
  has_look_ahead_ = false;
  unsigned nesting_level = 1;
  if (next_.GetBlockType() == CSSParserToken::kBlockStart) {
    nesting_level++;
  } else if (next_.GetBlockType() == CSSParserToken::kBlockEnd) {
    nesting_level--;
  }

  // Skip tokens until we see EOF or the closing brace.
  while (nesting_level != 0) {
    CSSParserToken token = tokenizer_.TokenizeSingle();
    if (token.IsEOF()) {
      break;
    } else if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      nesting_level++;
    } else if (token.GetBlockType() == CSSParserToken::kBlockEnd) {
      nesting_level--;
    }
  }
  offset_ = tokenizer_.Offset();
}

}  // namespace blink
