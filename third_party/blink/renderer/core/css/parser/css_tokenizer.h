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

class CORE_EXPORT CSSTokenizer {
  DISALLOW_NEW();

 public:
  // The overload with const String& holds on to a reference to the string.
  // (Most places, we probably don't need to do that, but fixing that would
  // require manual inspection.)
  explicit CSSTokenizer(const String&, wtf_size_t offset = 0);
  explicit CSSTokenizer(StringView, wtf_size_t offset = 0);
  CSSTokenizer(const CSSTokenizer&) = delete;
  CSSTokenizer& operator=(const CSSTokenizer&) = delete;

  Vector<CSSParserToken, 32> TokenizeToEOF();
  wtf_size_t TokenCount();

  // Like TokenizeToEOF(), but also returns the start byte for each token.
  // There's an extra offset at the very end that returns the end byte
  // of the last token, i.e., the length of the input string.
  // This matches the convention CSSParserTokenOffsets expects.
  std::pair<Vector<CSSParserToken, 32>, Vector<wtf_size_t, 32>>
  TokenizeToEOFWithOffsets();

  // The unicode-range descriptor invokes a special tokenizer
  // to solve a design mistake in CSS.
  //
  // https://drafts.csswg.org/css-syntax/#consume-unicode-range-value
  Vector<CSSParserToken, 32> TokenizeToEOFWithUnicodeRanges();

  wtf_size_t Offset() const { return input_.Offset(); }
  wtf_size_t PreviousOffset() const { return prev_offset_; }
  StringView StringRangeAt(wtf_size_t start, wtf_size_t length) const;
  const Vector<String>& StringPool() const { return string_pool_; }
  CSSParserToken TokenizeSingle();
  CSSParserToken TokenizeSingleWithComments();

  // If you want the returned CSSParserTokens' Value() to be valid beyond
  // the destruction of CSSTokenizer, you'll need to call PersistString()
  // to some longer-lived tokenizer (escaped string tokens may have
  // StringViews that refer to the string pool). The tokenizer
  // (*this, not the destination) is in an undefined state after this;
  // all you can do is destroy it.
  void PersistStrings(CSSTokenizer& destination);

  // See documentation near CSSParserTokenStream.
  CSSParserToken Restore(const CSSParserToken& next, wtf_size_t offset) {
    // Undo block stack mutation.
    if (next.GetBlockType() == CSSParserToken::BlockType::kBlockStart) {
      block_stack_.pop_back();
    } else if (next.GetBlockType() == CSSParserToken::BlockType::kBlockEnd) {
      static_assert(kLeftParenthesisToken == (kRightParenthesisToken - 1));
      static_assert(kLeftBracketToken == (kRightBracketToken - 1));
      static_assert(kLeftBraceToken == (kRightBraceToken - 1));
      block_stack_.push_back(
          static_cast<CSSParserTokenType>(next.GetType() - 1));
    }
    input_.Restore(offset);
    // Produce the post-restore lookahead token.
    return TokenizeSingle();
  }

 private:
  template <bool SkipComments, bool StoreOffset>
  ALWAYS_INLINE CSSParserToken NextToken();

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

  CSSTokenizerInputStream input_;
  Vector<CSSParserTokenType, 8> block_stack_;

  // We only allocate strings when escapes are used.
  Vector<String> string_pool_;

  friend class CSSParserTokenStream;

  wtf_size_t prev_offset_ = 0;
  wtf_size_t token_count_ = 0;

  bool unicode_ranges_allowed_ = false;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_TOKENIZER_H_
