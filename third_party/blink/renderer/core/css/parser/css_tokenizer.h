// Copyright 2014 The Chromium Authors
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

// A tokenizer which contains an already tokenized list of tokens. This can be
// used transparently in place of CSSTokenizer.
class CORE_EXPORT CachedCSSTokenizer {
 public:
  CachedCSSTokenizer(const String& input,
                     Vector<CSSParserToken> tokens,
                     Vector<wtf_size_t> offsets,
                     Vector<String> string_pool)
      : input_(input),
        tokens_(std::move(tokens)),
        offsets_(std::move(offsets)),
        string_pool_(std::move(string_pool)) {
    DCHECK_EQ(tokens_.size(), offsets_.size() - 1);
  }

  wtf_size_t Offset() const { return offsets_[index_]; }
  wtf_size_t PreviousOffset() const {
    if (index_ == 0)
      return 0;
    return offsets_[index_ - 1];
  }

  StringView StringRangeAt(wtf_size_t start, wtf_size_t length) const {
    return input_.RangeAt(start, length);
  }

  CSSParserToken TokenizeSingle() {
    while (true) {
      const CSSParserToken token = NextToken();
      if (token.GetType() == kCommentToken)
        continue;
      return token;
    }
  }

  CSSParserToken TokenizeSingleWithComments() { return NextToken(); }
  wtf_size_t TokenCount() { return index_; }

  std::unique_ptr<CachedCSSTokenizer> DuplicateForTesting() const;

 private:
  CSSParserToken NextToken() {
    if (index_ >= tokens_.size()) {
      DCHECK_EQ(tokens_.back().GetType(), kEOFToken);
      return tokens_.back();
    }
    return tokens_[index_++];
  }

  // Holds the source text of this sheet.
  CSSTokenizerInputStream input_;

  // The full list of tokens in the sheet.
  Vector<CSSParserToken> tokens_;

  // Offsets into the source text for each token.
  Vector<wtf_size_t> offsets_;

  // String pool to hold allocated strings, taken from CSSTokenizer.
  Vector<String> string_pool_;

  // The current token index.
  wtf_size_t index_ = 0;
};

class CORE_EXPORT CSSTokenizer {
  DISALLOW_NEW();

 public:
  // Immediately tokenizes the input string and saves the resulting tokens in
  // the returned tokenizer, which can be iterated on later.
  static std::unique_ptr<CachedCSSTokenizer> CreateCachedTokenizer(
      const String& input);

  explicit CSSTokenizer(const String&, wtf_size_t offset = 0);
  CSSTokenizer(const CSSTokenizer&) = delete;
  CSSTokenizer& operator=(const CSSTokenizer&) = delete;

  Vector<CSSParserToken, 32> TokenizeToEOF();
  wtf_size_t TokenCount();

  wtf_size_t Offset() const { return input_.Offset(); }
  wtf_size_t PreviousOffset() const { return prev_offset_; }
  StringView StringRangeAt(wtf_size_t start, wtf_size_t length) const;
  const Vector<String>& StringPool() const { return string_pool_; }
  CSSParserToken TokenizeSingle();
  CSSParserToken TokenizeSingleWithComments();

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

// A wrapper which can pass through calls to either a CachedCSSTokenizer or
// CSSTokenizer.
class CORE_EXPORT CSSTokenizerWrapper {
  DISALLOW_NEW();

 public:
  explicit CSSTokenizerWrapper(CSSTokenizer& tokenizer)
      : tokenizer_(&tokenizer) {}

  explicit CSSTokenizerWrapper(CachedCSSTokenizer& cached_tokenizer)
      : cached_tokenizer_(&cached_tokenizer) {}

  wtf_size_t Offset() const {
    return tokenizer_ ? tokenizer_->Offset() : cached_tokenizer_->Offset();
  }
  wtf_size_t PreviousOffset() const {
    return tokenizer_ ? tokenizer_->PreviousOffset()
                      : cached_tokenizer_->PreviousOffset();
  }
  StringView StringRangeAt(wtf_size_t start, wtf_size_t length) const {
    return tokenizer_ ? tokenizer_->StringRangeAt(start, length)
                      : cached_tokenizer_->StringRangeAt(start, length);
  }
  CSSParserToken TokenizeSingle() {
    return tokenizer_ ? tokenizer_->TokenizeSingle()
                      : cached_tokenizer_->TokenizeSingle();
  }
  CSSParserToken TokenizeSingleWithComments() {
    return tokenizer_ ? tokenizer_->TokenizeSingleWithComments()
                      : cached_tokenizer_->TokenizeSingleWithComments();
  }
  wtf_size_t TokenCount() {
    return tokenizer_ ? tokenizer_->TokenCount()
                      : cached_tokenizer_->TokenCount();
  }

 private:
  CSSTokenizer* tokenizer_ = nullptr;
  CachedCSSTokenizer* cached_tokenizer_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_TOKENIZER_H_
