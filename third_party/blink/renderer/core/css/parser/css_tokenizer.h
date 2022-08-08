// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_TOKENIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_TOKENIZER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer_input_stream.h"
#include "third_party/blink/renderer/core/html/parser/input_stream_preprocessor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <climits>

namespace blink {

class CSSTokenizerInputStream;

// Base class for all CSSTokenizer implementations.
class CORE_EXPORT CSSTokenizerBase {
 public:
  virtual ~CSSTokenizerBase() = default;

  virtual wtf_size_t Offset() const = 0;
  virtual wtf_size_t PreviousOffset() const = 0;
  virtual StringView StringRangeAt(wtf_size_t start,
                                   wtf_size_t length) const = 0;
  virtual CSSParserToken TokenizeSingle() = 0;
  virtual CSSParserToken TokenizeSingleWithComments() = 0;
  virtual wtf_size_t TokenCount() = 0;
};

class CORE_EXPORT CSSTokenizer : public CSSTokenizerBase {
 public:
  // Immediately tokenizes the input string and saves the resulting tokens in
  // the returned tokenizer, which can be iterated on later.
  static std::unique_ptr<CSSTokenizerBase> CreateCachedTokenizer(
      const String& input);

  explicit CSSTokenizer(const String&, wtf_size_t offset = 0);
  CSSTokenizer(const CSSTokenizer&) = delete;
  CSSTokenizer& operator=(const CSSTokenizer&) = delete;

  Vector<CSSParserToken, 32> TokenizeToEOF();
  wtf_size_t TokenCount() override;

  wtf_size_t Offset() const override { return input_.Offset(); }
  wtf_size_t PreviousOffset() const override { return prev_offset_; }
  StringView StringRangeAt(wtf_size_t start, wtf_size_t length) const override;
  CSSParserToken TokenizeSingle() override;
  CSSParserToken TokenizeSingleWithComments() override;

 private:
  CSSParserToken NextToken();

  UChar Consume();
  void Reconsume(UChar);

  CSSParserToken ConsumeNumericToken();
  CSSParserToken ConsumeIdentLikeToken();
  CSSParserToken ConsumeNumber();
  CSSParserToken ConsumeStringTokenUntil(UChar);
  CSSParserToken ConsumeUnicodeRange();
  CSSParserToken ConsumeUrlToken();

  void ConsumeBadUrlRemnants();
  void ConsumeSingleWhitespaceIfNext();
  void ConsumeUntilCommentEndFound();

  bool ConsumeIfNext(UChar);
  StringView ConsumeName();
  UChar32 ConsumeEscape();

  bool NextTwoCharsAreValidEscape();
  bool NextCharsAreNumber(UChar);
  bool NextCharsAreNumber();
  bool NextCharsAreIdentifier(UChar);
  bool NextCharsAreIdentifier();

  CSSParserToken BlockStart(CSSParserTokenType);
  CSSParserToken BlockStart(CSSParserTokenType block_type,
                            CSSParserTokenType,
                            StringView);
  CSSParserToken BlockEnd(CSSParserTokenType, CSSParserTokenType start_type);

  CSSParserToken WhiteSpace(UChar);
  CSSParserToken LeftParenthesis(UChar);
  CSSParserToken RightParenthesis(UChar);
  CSSParserToken LeftBracket(UChar);
  CSSParserToken RightBracket(UChar);
  CSSParserToken LeftBrace(UChar);
  CSSParserToken RightBrace(UChar);
  CSSParserToken PlusOrFullStop(UChar);
  CSSParserToken Comma(UChar);
  CSSParserToken HyphenMinus(UChar);
  CSSParserToken Asterisk(UChar);
  CSSParserToken LessThan(UChar);
  CSSParserToken Solidus(UChar);
  CSSParserToken Colon(UChar);
  CSSParserToken SemiColon(UChar);
  CSSParserToken Hash(UChar);
  CSSParserToken CircumflexAccent(UChar);
  CSSParserToken DollarSign(UChar);
  CSSParserToken VerticalLine(UChar);
  CSSParserToken Tilde(UChar);
  CSSParserToken CommercialAt(UChar);
  CSSParserToken ReverseSolidus(UChar);
  CSSParserToken AsciiDigit(UChar);
  CSSParserToken LetterU(UChar);
  CSSParserToken NameStart(UChar);
  CSSParserToken StringStart(UChar);
  CSSParserToken EndOfFile(UChar);

  StringView RegisterString(const String&);

  using CodePoint = CSSParserToken (CSSTokenizer::*)(UChar);
  static const CodePoint kCodePoints[];

  CSSTokenizerInputStream input_;
  Vector<CSSParserTokenType, 8> block_stack_;

  // We only allocate strings when escapes are used.
  Vector<String> string_pool_;

  friend class CSSParserTokenStream;

  wtf_size_t prev_offset_ = 0;
  wtf_size_t token_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_TOKENIZER_H_
