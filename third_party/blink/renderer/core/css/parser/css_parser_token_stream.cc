// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"

namespace blink {

template <bool Raw>
StringView CSSParserTokenStreamImpl<Raw>::StringRangeAt(
    wtf_size_t start,
    wtf_size_t length) const {
  return tokenizer_.StringRangeAt(start, length);
}

template <bool Raw>
void CSSParserTokenStreamImpl<Raw>::ConsumeWhitespace() {
  while (Peek().GetType() == kWhitespaceToken) {
    UncheckedConsume();
  }
}

template <bool Raw>
CSSParserToken CSSParserTokenStreamImpl<Raw>::ConsumeIncludingWhitespace() {
  CSSParserToken result = Consume();
  ConsumeWhitespace();
  return result;
}

template <bool Raw>
bool CSSParserTokenStreamImpl<Raw>::ConsumeCommentOrNothing() {
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

template <bool Raw>
void CSSParserTokenStreamImpl<Raw>::UncheckedConsumeComponentValue() {
  DCHECK(HasLookAhead());

  // Have to use internal consume/peek in here because they can read past
  // start/end of blocks
  unsigned nesting_level = 0;
  do {
    const CSSParserToken& token = UncheckedConsumeInternal();
    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      nesting_level++;
    } else if (token.GetBlockType() == CSSParserToken::kBlockEnd) {
      nesting_level--;
    }
  } while (!PeekInternal().IsEOF() && nesting_level);
}

template <bool Raw>
void CSSParserTokenStreamImpl<Raw>::UncheckedSkipToEndOfBlock() {
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
    CSSParserToken token = TokenizeSingle();
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

template class CORE_EXPORT CSSParserTokenStreamImpl<false>;
template class CORE_EXPORT CSSParserTokenStreamImpl<true>;

}  // namespace blink
