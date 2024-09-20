// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/input_stream_preprocessor.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

#ifdef __SSE2__
#include <immintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

namespace blink {

CSSTokenizer::CSSTokenizer(StringView string, wtf_size_t offset)
    : input_(string) {
  // According to the spec, we should perform preprocessing here.
  // See: https://drafts.csswg.org/css-syntax/#input-preprocessing
  //
  // However, we can skip this step since:
  // * We're using HTML spaces (which accept \r and \f as a valid white space)
  // * Do not count white spaces
  // * CSSTokenizerInputStream::NextInputChar() replaces NULLs for replacement
  //   characters
  input_.Advance(offset);
}

StringView CSSTokenizer::StringRangeFrom(wtf_size_t start) const {
  return input_.RangeFrom(start);
}

StringView CSSTokenizer::StringRangeAt(wtf_size_t start,
                                       wtf_size_t length) const {
  return input_.RangeAt(start, length);
}

CSSParserToken CSSTokenizer::TokenizeSingle() {
  return NextToken</*SkipComments=*/true>();
}

CSSParserToken CSSTokenizer::TokenizeSingleWithComments() {
  return NextToken</*SkipComments=*/false>();
}

wtf_size_t CSSTokenizer::TokenCount() const {
  return token_count_;
}

void CSSTokenizer::Reconsume(UChar c) {
  input_.PushBack(c);
}

UChar CSSTokenizer::Consume() {
  UChar current = input_.NextInputChar();
  input_.Advance();
  return current;
}

CSSParserToken CSSTokenizer::BlockStart(CSSParserTokenType type) {
  block_stack_.push_back(type);
  return CSSParserToken(type, CSSParserToken::kBlockStart);
}

CSSParserToken CSSTokenizer::BlockStart(CSSParserTokenType block_type,
                                        CSSParserTokenType type,
                                        StringView name) {
  block_stack_.push_back(block_type);
  return CSSParserToken(type, name, CSSParserToken::kBlockStart);
}

CSSParserToken CSSTokenizer::BlockEnd(CSSParserTokenType type,
                                      CSSParserTokenType start_type) {
  if (!block_stack_.empty() && block_stack_.back() == start_type) {
    block_stack_.pop_back();
    return CSSParserToken(type, CSSParserToken::kBlockEnd);
  }
  return CSSParserToken(type);
}

CSSParserToken CSSTokenizer::HyphenMinus(UChar cc) {
  if (NextCharsAreNumber(cc)) {
    Reconsume(cc);
    return ConsumeNumericToken();
  }
  if (input_.PeekWithoutReplacement(0) == '-' &&
      input_.PeekWithoutReplacement(1) == '>') {
    input_.Advance(2);
    return CSSParserToken(kCDCToken);
  }
  if (NextCharsAreIdentifier(cc)) {
    Reconsume(cc);
    return ConsumeIdentLikeToken();
  }
  return CSSParserToken(kDelimiterToken, cc);
}

CSSParserToken CSSTokenizer::Hash(UChar cc) {
  UChar next_char = input_.PeekWithoutReplacement(0);
  if (IsNameCodePoint(next_char) ||
      TwoCharsAreValidEscape(next_char, input_.PeekWithoutReplacement(1))) {
    HashTokenType type =
        NextCharsAreIdentifier() ? kHashTokenId : kHashTokenUnrestricted;
    return CSSParserToken(type, ConsumeName());
  }

  return CSSParserToken(kDelimiterToken, cc);
}

CSSParserToken CSSTokenizer::LetterU(UChar cc) {
  if (unicode_ranges_allowed_ && input_.PeekWithoutReplacement(0) == '+' &&
      (IsASCIIHexDigit(input_.PeekWithoutReplacement(1)) ||
       input_.PeekWithoutReplacement(1) == '?')) {
    input_.Advance();
    return ConsumeUnicodeRange();
  }
  Reconsume(cc);
  return ConsumeIdentLikeToken();
}

template <bool SkipComments>
CSSParserToken CSSTokenizer::NextToken() {
  do {
    prev_offset_ = input_.Offset();
    // Unlike the HTMLTokenizer, the CSS Syntax spec is written
    // as a stateless, (fixed-size) look-ahead tokenizer.
    // We could move to the stateful model and instead create
    // states for all the "next 3 codepoints are X" cases.
    // State-machine tokenizers are easier to write to handle
    // incremental tokenization of partial sources.
    // However, for now we follow the spec exactly.
    UChar cc = Consume();
    ++token_count_;

    switch (cc) {
      case 0:
        return CSSParserToken(kEOFToken);
      case '\t':
      case '\n':
      case '\f':
      case '\r':
      case ' ':
        input_.AdvanceUntilNonWhitespace();
        return CSSParserToken(kWhitespaceToken);
      case '\'':
      case '"':
        return ConsumeStringTokenUntil(cc);
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        Reconsume(cc);
        return ConsumeNumericToken();
      case '(':
        return BlockStart(kLeftParenthesisToken);
      case ')':
        return BlockEnd(kRightParenthesisToken, kLeftParenthesisToken);
      case '[':
        return BlockStart(kLeftBracketToken);
      case ']':
        return BlockEnd(kRightBracketToken, kLeftBracketToken);
      case '{':
        return BlockStart(kLeftBraceToken);
      case '}':
        return BlockEnd(kRightBraceToken, kLeftBraceToken);
      case '+':
      case '.':
        if (NextCharsAreNumber(cc)) {
          Reconsume(cc);
          return ConsumeNumericToken();
        }
        return CSSParserToken(kDelimiterToken, cc);
      case '-':
        return HyphenMinus(cc);
      case '*':
        if (ConsumeIfNext('=')) {
          return CSSParserToken(kSubstringMatchToken);
        }
        return CSSParserToken(kDelimiterToken, '*');
      case '<':
        if (input_.PeekWithoutReplacement(0) == '!' &&
            input_.PeekWithoutReplacement(1) == '-' &&
            input_.PeekWithoutReplacement(2) == '-') {
          input_.Advance(3);
          return CSSParserToken(kCDOToken);
        }
        return CSSParserToken(kDelimiterToken, '<');
      case ',':
        return CSSParserToken(kCommaToken);
      case '/':
        if (ConsumeIfNext('*')) {
          ConsumeUntilCommentEndFound();
          if (SkipComments) {
            break;  // Read another token.
          } else {
            return CSSParserToken(kCommentToken);
          }
        }
        return CSSParserToken(kDelimiterToken, cc);
      case '\\':
        if (TwoCharsAreValidEscape(cc, input_.PeekWithoutReplacement(0))) {
          Reconsume(cc);
          return ConsumeIdentLikeToken();
        }
        return CSSParserToken(kDelimiterToken, cc);
      case ':':
        return CSSParserToken(kColonToken);
      case ';':
        return CSSParserToken(kSemicolonToken);
      case '#':
        return Hash(cc);
      case '^':
        if (ConsumeIfNext('=')) {
          return CSSParserToken(kPrefixMatchToken);
        }
        return CSSParserToken(kDelimiterToken, '^');
      case '$':
        if (ConsumeIfNext('=')) {
          return CSSParserToken(kSuffixMatchToken);
        }
        return CSSParserToken(kDelimiterToken, '$');
      case '|':
        if (ConsumeIfNext('=')) {
          return CSSParserToken(kDashMatchToken);
        }
        if (ConsumeIfNext('|')) {
          return CSSParserToken(kColumnToken);
        }
        return CSSParserToken(kDelimiterToken, '|');
      case '~':
        if (ConsumeIfNext('=')) {
          return CSSParserToken(kIncludeMatchToken);
        }
        return CSSParserToken(kDelimiterToken, '~');
      case '@':
        if (NextCharsAreIdentifier()) {
          return CSSParserToken(kAtKeywordToken, ConsumeName());
        }
        return CSSParserToken(kDelimiterToken, '@');
      case 'u':
      case 'U':
        return LetterU(cc);
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
      case 11:
      case 14:
      case 15:
      case 16:
      case 17:
      case 18:
      case 19:
      case 20:
      case 21:
      case 22:
      case 23:
      case 24:
      case 25:
      case 26:
      case 27:
      case 28:
      case 29:
      case 30:
      case 31:
      case '!':
      case '%':
      case '&':
      case '=':
      case '>':
      case '?':
      case '`':
      case 127:
        return CSSParserToken(kDelimiterToken, cc);
      default:
        Reconsume(cc);
        return ConsumeIdentLikeToken();
    }
  } while (SkipComments);
}

// This method merges the following spec sections for efficiency
// http://www.w3.org/TR/css3-syntax/#consume-a-number
// http://www.w3.org/TR/css3-syntax/#convert-a-string-to-a-number
CSSParserToken CSSTokenizer::ConsumeNumber() {
  DCHECK(NextCharsAreNumber());

  NumericValueType type = kIntegerValueType;
  NumericSign sign = kNoSign;
  unsigned number_length = 0;
  unsigned sign_length = 0;

  UChar next = input_.PeekWithoutReplacement(0);
  if (next == '+') {
    ++number_length;
    ++sign_length;
    sign = kPlusSign;
  } else if (next == '-') {
    ++number_length;
    ++sign_length;
    sign = kMinusSign;
  }

  number_length = input_.SkipWhilePredicate<IsASCIIDigit>(number_length);
  next = input_.PeekWithoutReplacement(number_length);
  if (next == '.' &&
      IsASCIIDigit(input_.PeekWithoutReplacement(number_length + 1))) {
    type = kNumberValueType;
    number_length = input_.SkipWhilePredicate<IsASCIIDigit>(number_length + 2);
    next = input_.PeekWithoutReplacement(number_length);
  }

  if (next == 'E' || next == 'e') {
    next = input_.PeekWithoutReplacement(number_length + 1);
    if (IsASCIIDigit(next)) {
      type = kNumberValueType;
      number_length =
          input_.SkipWhilePredicate<IsASCIIDigit>(number_length + 1);
    } else if ((next == '+' || next == '-') &&
               IsASCIIDigit(input_.PeekWithoutReplacement(number_length + 2))) {
      type = kNumberValueType;
      number_length =
          input_.SkipWhilePredicate<IsASCIIDigit>(number_length + 3);
    }
  }

  double value;
  if (type == kIntegerValueType) {
    // Fast path.
    value = input_.GetNaturalNumberAsDouble(sign_length, number_length);
    if (sign == kMinusSign) {
      value = -value;
    }
    DCHECK_EQ(value, input_.GetDouble(0, number_length));
    input_.Advance(number_length);
  } else {
    value = input_.GetDouble(0, number_length);
    input_.Advance(number_length);
  }

  return CSSParserToken(kNumberToken, value, type, sign);
}

// http://www.w3.org/TR/css3-syntax/#consume-a-numeric-token
CSSParserToken CSSTokenizer::ConsumeNumericToken() {
  CSSParserToken token = ConsumeNumber();
  if (NextCharsAreIdentifier()) {
    token.ConvertToDimensionWithUnit(ConsumeName());
  } else if (ConsumeIfNext('%')) {
    token.ConvertToPercentage();
  }
  return token;
}

// https://drafts.csswg.org/css-syntax/#consume-ident-like-token
CSSParserToken CSSTokenizer::ConsumeIdentLikeToken() {
  StringView name = ConsumeName();
  if (ConsumeIfNext('(')) {
    if (EqualIgnoringASCIICase(name, "url")) {
      // The spec is slightly different so as to avoid dropping whitespace
      // tokens, but they wouldn't be used and this is easier.
      input_.AdvanceUntilNonWhitespace();
      UChar next = input_.PeekWithoutReplacement(0);
      if (next != '"' && next != '\'') {
        return ConsumeUrlToken();
      }
    }
    return BlockStart(kLeftParenthesisToken, kFunctionToken, name);
  }
  return CSSParserToken(kIdentToken, name);
}

// https://drafts.csswg.org/css-syntax/#consume-a-string-token
CSSParserToken CSSTokenizer::ConsumeStringTokenUntil(UChar ending_code_point) {
  // Strings without escapes get handled without allocations
  for (unsigned size = 0;; size++) {
    UChar cc = input_.PeekWithoutReplacement(size);
    if (cc == ending_code_point) {
      unsigned start_offset = input_.Offset();
      input_.Advance(size + 1);
      return CSSParserToken(kStringToken, input_.RangeAt(start_offset, size));
    }
    if (IsCSSNewLine(cc)) {
      input_.Advance(size);
      return CSSParserToken(kBadStringToken);
    }
    if (cc == '\0' || cc == '\\') {
      break;
    }
  }

  StringBuilder output;
  while (true) {
    UChar cc = Consume();
    if (cc == ending_code_point || cc == kEndOfFileMarker) {
      return CSSParserToken(kStringToken,
                            RegisterString(output.ReleaseString()));
    }
    if (IsCSSNewLine(cc)) {
      Reconsume(cc);
      return CSSParserToken(kBadStringToken);
    }
    if (cc == '\\') {
      if (input_.NextInputChar() == kEndOfFileMarker) {
        continue;
      }
      if (IsCSSNewLine(input_.PeekWithoutReplacement(0))) {
        ConsumeSingleWhitespaceIfNext();  // This handles \r\n for us
      } else {
        output.Append(ConsumeEscape());
      }
    } else {
      output.Append(cc);
    }
  }
}

CSSParserToken CSSTokenizer::ConsumeUnicodeRange() {
  DCHECK(IsASCIIHexDigit(input_.PeekWithoutReplacement(0)) ||
         input_.PeekWithoutReplacement(0) == '?');
  int length_remaining = 6;
  UChar32 start = 0;

  while (length_remaining &&
         IsASCIIHexDigit(input_.PeekWithoutReplacement(0))) {
    start = start * 16 + ToASCIIHexValue(Consume());
    --length_remaining;
  }

  UChar32 end = start;
  if (length_remaining && ConsumeIfNext('?')) {
    do {
      start *= 16;
      end = end * 16 + 0xF;
      --length_remaining;
    } while (length_remaining && ConsumeIfNext('?'));
  } else if (input_.PeekWithoutReplacement(0) == '-' &&
             IsASCIIHexDigit(input_.PeekWithoutReplacement(1))) {
    input_.Advance();
    length_remaining = 6;
    end = 0;
    do {
      end = end * 16 + ToASCIIHexValue(Consume());
      --length_remaining;
    } while (length_remaining &&
             IsASCIIHexDigit(input_.PeekWithoutReplacement(0)));
  }

  return CSSParserToken(kUnicodeRangeToken, start, end);
}

// https://drafts.csswg.org/css-syntax/#non-printable-code-point
static bool IsNonPrintableCodePoint(UChar cc) {
  return (cc >= '\0' && cc <= '\x8') || cc == '\xb' ||
         (cc >= '\xe' && cc <= '\x1f') || cc == '\x7f';
}

// https://drafts.csswg.org/css-syntax/#consume-url-token
CSSParserToken CSSTokenizer::ConsumeUrlToken() {
  input_.AdvanceUntilNonWhitespace();

  // URL tokens without escapes get handled without allocations
  for (unsigned size = 0;; size++) {
    UChar cc = input_.PeekWithoutReplacement(size);
    if (cc == ')') {
      unsigned start_offset = input_.Offset();
      input_.Advance(size + 1);
      return CSSParserToken(kUrlToken, input_.RangeAt(start_offset, size));
    }
    if (cc <= ' ' || cc == '\\' || cc == '"' || cc == '\'' || cc == '(' ||
        cc == '\x7f') {
      break;
    }
  }

  StringBuilder result;
  while (true) {
    UChar cc = Consume();
    if (cc == ')' || cc == kEndOfFileMarker) {
      return CSSParserToken(kUrlToken, RegisterString(result.ReleaseString()));
    }

    if (IsHTMLSpace(cc)) {
      input_.AdvanceUntilNonWhitespace();
      if (ConsumeIfNext(')') || input_.NextInputChar() == kEndOfFileMarker) {
        return CSSParserToken(kUrlToken,
                              RegisterString(result.ReleaseString()));
      }
      break;
    }

    if (cc == '"' || cc == '\'' || cc == '(' || IsNonPrintableCodePoint(cc)) {
      break;
    }

    if (cc == '\\') {
      if (TwoCharsAreValidEscape(cc, input_.PeekWithoutReplacement(0))) {
        result.Append(ConsumeEscape());
        continue;
      }
      break;
    }

    result.Append(cc);
  }

  ConsumeBadUrlRemnants();
  return CSSParserToken(kBadUrlToken);
}

// https://drafts.csswg.org/css-syntax/#consume-the-remnants-of-a-bad-url
void CSSTokenizer::ConsumeBadUrlRemnants() {
  while (true) {
    UChar cc = Consume();
    if (cc == ')' || cc == kEndOfFileMarker) {
      return;
    }
    if (TwoCharsAreValidEscape(cc, input_.PeekWithoutReplacement(0))) {
      ConsumeEscape();
    }
  }
}

void CSSTokenizer::ConsumeSingleWhitespaceIfNext() {
  blink::ConsumeSingleWhitespaceIfNext(input_);
}

void CSSTokenizer::ConsumeUntilCommentEndFound() {
  UChar c = Consume();
  while (true) {
    if (c == kEndOfFileMarker) {
      return;
    }
    if (c != '*') {
      c = Consume();
      continue;
    }
    c = Consume();
    if (c == '/') {
      return;
    }
  }
}

bool CSSTokenizer::ConsumeIfNext(UChar character) {
  // Since we're not doing replacement we can't tell the difference from
  // a NUL in the middle and the kEndOfFileMarker, so character must not be
  // NUL.
  DCHECK(character);
  if (input_.PeekWithoutReplacement(0) == character) {
    input_.Advance();
    return true;
  }
  return false;
}

// http://www.w3.org/TR/css3-syntax/#consume-name
//
// Consumes a name, which is defined as a contiguous sequence of name code
// points (see IsNameCodePoint()), possibly with escapes. We stop at the first
// thing that is _not_ a name code point (or the end of a string); if that is a
// backslash, we hand over to the more complete and slower blink::ConsumeName().
// If not, we can send back the relevant substring of the input, without any
// allocations.
//
// If SIMD is available (we support only SSE2 and NEON), we do this 16 and 16
// bytes at a time, generally giving a speed boost except for very short names.
// (We don't get short-circuiting, and we need some extra setup to load
// constants, but we also don't get a lot of branches per byte that we
// consider.)
//
// The checking for \0 is a bit odd; \0 is sometimes used as an EOF marker
// internal to this code, so we need to call into blink::ConsumeName()
// to escape it (into a Unicode replacement character) if we should see it.
StringView CSSTokenizer::ConsumeName() {
  StringView buffer = input_.Peek();

  unsigned size = 0;
#if defined(__SSE2__) || defined(__ARM_NEON__)
  if (buffer.Is8Bit()) {
    const LChar* ptr = buffer.Characters8();
    while (size + 16 <= buffer.length()) {
      int8_t b __attribute__((vector_size(16)));
      memcpy(&b, ptr + size, sizeof(b));

      // Exactly the same as IsNameCodePoint(), except the IsASCII() part,
      // which we deal with below. Note that we compute the inverted condition,
      // since __builtin_ctz wants to find the first 1-bit, not the first 0-bit.
      auto non_name_mask = ((b | 0x20) < 'a' || (b | 0x20) > 'z') && b != '_' &&
                           b != '-' && (b < '0' || b > '9');
#ifdef __SSE2__
      // pmovmskb extracts only the top bit and ignores the rest,
      // so to implement the IsASCII() test, which for LChar only
      // tests whether the top bit is set, we don't need a compare;
      // we can just rely on the top bit directly (using a PANDN).
      uint16_t bits =
          _mm_movemask_epi8(reinterpret_cast<__m128i>(non_name_mask & ~b));
      if (bits == 0) {
        size += 16;
        continue;
      }

      // We found either the end, or a sign that we need escape-aware parsing.
      size += __builtin_ctz(bits);
#else  // __ARM_NEON__

      // NEON doesn't have pmovmskb, so we'll need to do the actual compare
      // (or something similar, like shifting). Now the mask is either all-zero
      // or all-one for each byte, so we can use the code from
      // https://community.arm.com/arm-community-blogs/b/infrastructure-solutions-blog/posts/porting-x86-vector-bitmask-optimizations-to-arm-neon
      non_name_mask = non_name_mask && (b >= 0);
      uint8x8_t narrowed_mask =
          vshrn_n_u16(vreinterpretq_u16_s8(non_name_mask), 4);
      uint64_t bits = vget_lane_u64(vreinterpret_u64_u8(narrowed_mask), 0);
      if (bits == 0) {
        size += 16;
        continue;
      }

      // We found either the end, or a sign that we need escape-aware parsing.
      size += __builtin_ctzll(bits) >> 2;
#endif
      if (ptr[size] == '\0' || ptr[size] == '\\') {
        // We need escape-aware parsing.
        return RegisterString(blink::ConsumeName(input_));
      } else {
        input_.Advance(size);
        return StringView(buffer, 0, size);
      }
    }
    // Fall back to the slow path for the last <= 15 bytes of the string.
  }
#endif  // SIMD

  // Slow path for non-UTF-8 and tokens near the end of the string.
  for (; size < buffer.length(); ++size) {
    UChar cc = buffer[size];
    if (!IsNameCodePoint(cc)) {
      // End of this token, but not end of the string.
      if (cc == '\0' || cc == '\\') {
        // We need escape-aware parsing.
        return RegisterString(blink::ConsumeName(input_));
      } else {
        // Names without escapes get handled without allocations
        input_.Advance(size);
        return StringView(buffer, 0, size);
      }
    }
  }

  // The entire rest of the string is a name.
  input_.Advance(size);
  return buffer;
}

// https://drafts.csswg.org/css-syntax/#consume-an-escaped-code-point
UChar32 CSSTokenizer::ConsumeEscape() {
  return blink::ConsumeEscape(input_);
}

bool CSSTokenizer::NextTwoCharsAreValidEscape() {
  return TwoCharsAreValidEscape(input_.PeekWithoutReplacement(0),
                                input_.PeekWithoutReplacement(1));
}

// http://www.w3.org/TR/css3-syntax/#starts-with-a-number
bool CSSTokenizer::NextCharsAreNumber(UChar first) {
  UChar second = input_.PeekWithoutReplacement(0);
  if (IsASCIIDigit(first)) {
    return true;
  }
  if (first == '+' || first == '-') {
    return ((IsASCIIDigit(second)) ||
            (second == '.' && IsASCIIDigit(input_.PeekWithoutReplacement(1))));
  }
  if (first == '.') {
    return (IsASCIIDigit(second));
  }
  return false;
}

bool CSSTokenizer::NextCharsAreNumber() {
  UChar first = Consume();
  bool are_number = NextCharsAreNumber(first);
  Reconsume(first);
  return are_number;
}

// https://drafts.csswg.org/css-syntax/#would-start-an-identifier
bool CSSTokenizer::NextCharsAreIdentifier(UChar first) {
  return blink::NextCharsAreIdentifier(first, input_);
}

bool CSSTokenizer::NextCharsAreIdentifier() {
  UChar first = Consume();
  bool are_identifier = NextCharsAreIdentifier(first);
  Reconsume(first);
  return are_identifier;
}

StringView CSSTokenizer::RegisterString(const String& string) {
  string_pool_.push_back(string);
  return string;
}

}  // namespace blink
