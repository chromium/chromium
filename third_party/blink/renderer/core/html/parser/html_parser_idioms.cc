/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

#include <limits>

namespace blink {

template <typename CharType>
static String StripLeadingAndTrailingHTMLSpaces(String string,
                                                const CharType* characters,
                                                unsigned length) {
  unsigned num_leading_spaces = 0;
  unsigned num_trailing_spaces = 0;

  for (; num_leading_spaces < length; ++num_leading_spaces) {
    if (IsNotHTMLSpace<CharType>(characters[num_leading_spaces]))
      break;
  }

  if (num_leading_spaces == length)
    return string.IsNull() ? string : g_empty_atom.GetString();

  for (; num_trailing_spaces < length; ++num_trailing_spaces) {
    if (IsNotHTMLSpace<CharType>(characters[length - num_trailing_spaces - 1]))
      break;
  }

  DCHECK_LT(num_leading_spaces + num_trailing_spaces, length);

  if (!(num_leading_spaces | num_trailing_spaces))
    return string;

  return string.Substring(num_leading_spaces,
                          length - (num_leading_spaces + num_trailing_spaces));
}

String StripLeadingAndTrailingHTMLSpaces(const String& string) {
  unsigned length = string.length();

  if (!length)
    return string.IsNull() ? string : g_empty_atom.GetString();

  if (string.Is8Bit())
    return StripLeadingAndTrailingHTMLSpaces<LChar>(
        string, string.Characters8(), length);

  return StripLeadingAndTrailingHTMLSpaces<UChar>(string, string.Characters16(),
                                                  length);
}

String SerializeForNumberType(const Decimal& number) {
  if (number.IsZero()) {
    // Decimal::toString appends exponent, e.g. "0e-18"
    return number.IsNegative() ? "-0" : "0";
  }
  return number.ToString();
}

String SerializeForNumberType(double number) {
  // According to HTML5, "the best representation of the number n as a floating
  // point number" is a string produced by applying ToString() to n.
  return String::NumberToStringECMAScript(number);
}

Decimal ParseToDecimalForNumberType(const String& string,
                                    const Decimal& fallback_value) {
  // http://www.whatwg.org/specs/web-apps/current-work/#floating-point-numbers
  // and parseToDoubleForNumberType String::toDouble() accepts leading + and
  // whitespace characters, which are not valid here.
  const UChar first_character = string[0];
  if (first_character != '-' && first_character != '.' &&
      !IsASCIIDigit(first_character))
    return fallback_value;

  const Decimal value = Decimal::FromString(string);
  if (!value.IsFinite())
    return fallback_value;

  // Numbers are considered finite IEEE 754 Double-precision floating point
  // values.
  const Decimal double_max =
      Decimal::FromDouble(std::numeric_limits<double>::max());
  if (value < -double_max || value > double_max)
    return fallback_value;

  // We return +0 for -0 case.
  return value.IsZero() ? Decimal(0) : value;
}

static double CheckDoubleValue(double value,
                               bool valid,
                               double fallback_value) {
  if (!valid)
    return fallback_value;

  // NaN and infinity are considered valid by String::toDouble, but not valid
  // here.
  if (!std::isfinite(value))
    return fallback_value;

  // Numbers are considered finite IEEE 754 Double-precision floating point
  // values.
  if (-std::numeric_limits<double>::max() > value ||
      value > std::numeric_limits<double>::max())
    return fallback_value;

  // The following expression converts -0 to +0.
  return value ? value : 0;
}

double ParseToDoubleForNumberType(const String& string, double fallback_value) {
  // http://www.whatwg.org/specs/web-apps/current-work/#floating-point-numbers
  // String::toDouble() accepts leading + and whitespace characters, which are
  // not valid here.
  UChar first_character = string[0];
  if (first_character != '-' && first_character != '.' &&
      !IsASCIIDigit(first_character))
    return fallback_value;
  if (string.EndsWith('.'))
    return fallback_value;

  bool valid = false;
  double value = string.ToDouble(&valid);
  return CheckDoubleValue(value, valid, fallback_value);
}

template <typename CharacterType>
static bool ParseHTMLIntegerInternal(const CharacterType* position,
                                     const CharacterType* end,
                                     int& value) {
  // Step 4
  SkipWhile<CharacterType, IsHTMLSpace<CharacterType>>(position, end);

  // Step 5
  if (position == end)
    return false;
  DCHECK_LT(position, end);

  bool ok;
  WTF::NumberParsingOptions options(
      WTF::NumberParsingOptions::kAcceptTrailingGarbage |
      WTF::NumberParsingOptions::kAcceptLeadingPlus);
  int wtf_value = CharactersToInt(position, end - position, options, &ok);
  if (ok)
    value = wtf_value;
  return ok;
}

// http://www.whatwg.org/specs/web-apps/current-work/#rules-for-parsing-integers
bool ParseHTMLInteger(const String& input, int& value) {
  // Step 1
  // Step 2
  unsigned length = input.length();
  if (!length || input.Is8Bit()) {
    const LChar* start = input.Characters8();
    return ParseHTMLIntegerInternal(start, start + length, value);
  }

  const UChar* start = input.Characters16();
  return ParseHTMLIntegerInternal(start, start + length, value);
}

template <typename CharacterType>
static WTF::NumberParsingResult ParseHTMLNonNegativeIntegerInternal(
    const CharacterType* position,
    const CharacterType* end,
    unsigned& value) {
  // This function is an implementation of the following algorithm:
  // https://html.spec.whatwg.org/C/#rules-for-parsing-non-negative-integers
  // However, in order to support integers >= 2^31, we fold [1] into this.
  // 'Step N' in the following comments refers to [1].
  //
  // [1]
  // https://html.spec.whatwg.org/C/#rules-for-parsing-integers

  // Step 4: Skip whitespace.
  SkipWhile<CharacterType, IsHTMLSpace<CharacterType>>(position, end);

  // Step 5: If position is past the end of input, return an error.
  if (position == end)
    return WTF::NumberParsingResult::kError;
  DCHECK_LT(position, end);

  WTF::NumberParsingResult result;
  WTF::NumberParsingOptions options(
      WTF::NumberParsingOptions::kAcceptTrailingGarbage |
      WTF::NumberParsingOptions::kAcceptLeadingPlus |
      WTF::NumberParsingOptions::kAcceptMinusZeroForUnsigned);
  unsigned wtf_value =
      CharactersToUInt(position, end - position, options, &result);
  if (result == WTF::NumberParsingResult::kSuccess)
    value = wtf_value;
  return result;
}

static WTF::NumberParsingResult ParseHTMLNonNegativeIntegerInternal(
    const String& input,
    unsigned& value) {
  unsigned length = input.length();
  if (length == 0)
    return WTF::NumberParsingResult::kError;
  if (input.Is8Bit()) {
    const LChar* start = input.Characters8();
    return ParseHTMLNonNegativeIntegerInternal(start, start + length, value);
  }

  const UChar* start = input.Characters16();
  return ParseHTMLNonNegativeIntegerInternal(start, start + length, value);
}

// https://html.spec.whatwg.org/C/#rules-for-parsing-non-negative-integers
bool ParseHTMLNonNegativeInteger(const String& input, unsigned& value) {
  return ParseHTMLNonNegativeIntegerInternal(input, value) ==
         WTF::NumberParsingResult::kSuccess;
}

bool ParseHTMLClampedNonNegativeInteger(const String& input,
                                        unsigned min,
                                        unsigned max,
                                        unsigned& value) {
  unsigned parsed_value;
  switch (ParseHTMLNonNegativeIntegerInternal(input, parsed_value)) {
    case WTF::NumberParsingResult::kError:
      return false;
    case WTF::NumberParsingResult::kOverflowMin:
      NOTREACHED() << input;
      return false;
    case WTF::NumberParsingResult::kOverflowMax:
      value = max;
      return true;
    case WTF::NumberParsingResult::kSuccess:
      value = std::max(min, std::min(parsed_value, max));
      return true;
  }
  return false;
}

template <typename CharacterType>
static bool IsSpaceOrDelimiter(CharacterType c) {
  return IsHTMLSpace(c) || c == ',' || c == ';';
}

template <typename CharacterType>
static bool IsNotSpaceDelimiterOrNumberStart(CharacterType c) {
  return !(IsSpaceOrDelimiter(c) || IsASCIIDigit(c) || c == '.' || c == '-');
}

template <typename CharacterType>
static Vector<double> ParseHTMLListOfFloatingPointNumbersInternal(
    const CharacterType* position,
    const CharacterType* end) {
  Vector<double> numbers;
  SkipWhile<CharacterType, IsSpaceOrDelimiter>(position, end);

  while (position < end) {
    SkipWhile<CharacterType, IsNotSpaceDelimiterOrNumberStart>(position, end);

    const CharacterType* unparsed_number_start = position;
    SkipUntil<CharacterType, IsSpaceOrDelimiter>(position, end);

    size_t parsed_length = 0;
    double number = CharactersToDouble(
        unparsed_number_start, position - unparsed_number_start, parsed_length);
    numbers.push_back(CheckDoubleValue(number, parsed_length != 0, 0));

    SkipWhile<CharacterType, IsSpaceOrDelimiter>(position, end);
  }
  return numbers;
}

// https://html.spec.whatwg.org/C/#rules-for-parsing-a-list-of-floating-point-numbers
Vector<double> ParseHTMLListOfFloatingPointNumbers(const String& input) {
  unsigned length = input.length();
  if (!length || input.Is8Bit())
    return ParseHTMLListOfFloatingPointNumbersInternal(
        input.Characters8(), input.Characters8() + length);
  return ParseHTMLListOfFloatingPointNumbersInternal(
      input.Characters16(), input.Characters16() + length);
}

static const char kCharsetString[] = "charset";
static const size_t kCharsetLength = sizeof("charset") - 1;

// https://html.spec.whatwg.org/C/#extracting-character-encodings-from-meta-elements
String ExtractCharset(const String& value) {
  wtf_size_t pos = 0;
  unsigned length = value.length();

  while (pos < length) {
    pos = value.FindIgnoringASCIICase(kCharsetString, pos);
    if (pos == kNotFound)
      break;

    pos += kCharsetLength;

    // Skip whitespace.
    while (pos < length && value[pos] <= ' ')
      ++pos;

    if (value[pos] != '=')
      continue;

    ++pos;

    while (pos < length && value[pos] <= ' ')
      ++pos;

    char quote_mark = 0;
    if (pos < length && (value[pos] == '"' || value[pos] == '\'')) {
      quote_mark = static_cast<char>(value[pos++]);
      DCHECK(!(quote_mark & 0x80));
    }

    if (pos == length)
      break;

    unsigned end = pos;
    while (end < length &&
           ((quote_mark && value[end] != quote_mark) ||
            (!quote_mark && value[end] > ' ' && value[end] != '"' &&
             value[end] != '\'' && value[end] != ';')))
      ++end;

    if (quote_mark && (end == length))
      break;  // Close quote not found.

    return value.Substring(pos, end - pos);
  }

  return "";
}

enum class MetaAttribute {
  kNone,
  kCharset,
  kPragma,
};

WTF::TextEncoding EncodingFromMetaAttributes(
    const HTMLAttributeList& attributes) {
  bool got_pragma = false;
  bool has_charset = false;
  MetaAttribute mode = MetaAttribute::kNone;
  String charset;

  for (const auto& html_attribute : attributes) {
    const String& attribute_name = html_attribute.first;
    const AtomicString& attribute_value = AtomicString(html_attribute.second);

    if (ThreadSafeMatch(attribute_name, html_names::kHttpEquivAttr)) {
      if (DeprecatedEqualIgnoringCase(attribute_value, "content-type"))
        got_pragma = true;
    } else if (ThreadSafeMatch(attribute_name, html_names::kCharsetAttr)) {
      has_charset = true;
      charset = attribute_value;
      mode = MetaAttribute::kCharset;
    } else if (!has_charset &&
               ThreadSafeMatch(attribute_name, html_names::kContentAttr)) {
      charset = ExtractCharset(attribute_value);
      if (charset.length())
        mode = MetaAttribute::kPragma;
    }
  }

  if (mode == MetaAttribute::kCharset ||
      (mode == MetaAttribute::kPragma && got_pragma))
    return WTF::TextEncoding(StripLeadingAndTrailingHTMLSpaces(charset));

  return WTF::TextEncoding();
}

static bool ThreadSafeEqual(const StringImpl* a, const StringImpl* b) {
  if (a == b)
    return true;
  if (a->GetHash() != b->GetHash())
    return false;
  return EqualNonNull(a, b);
}

bool ThreadSafeMatch(const QualifiedName& a, const QualifiedName& b) {
  return ThreadSafeEqual(a.LocalName().Impl(), b.LocalName().Impl());
}

bool ThreadSafeMatch(const String& local_name, const QualifiedName& q_name) {
  return ThreadSafeEqual(local_name.Impl(), q_name.LocalName().Impl());
}

template <typename CharType>
inline StringImpl* FindStringIfStatic(const CharType* characters,
                                      unsigned length) {
  // We don't need to try hashing if we know the string is too long.
  if (length > StringImpl::HighestStaticStringLength())
    return nullptr;
  // computeHashAndMaskTop8Bits is the function StringImpl::hash() uses.
  unsigned hash = StringHasher::ComputeHashAndMaskTop8Bits(characters, length);
  const WTF::StaticStringsTable& table = StringImpl::AllStaticStrings();
  DCHECK(!table.IsEmpty());

  WTF::StaticStringsTable::const_iterator it = table.find(hash);
  if (it == table.end())
    return nullptr;
  // It's possible to have hash collisions between arbitrary strings and known
  // identifiers (e.g. "bvvfg" collides with "script"). However ASSERTs in
  // StringImpl::createStatic guard against there ever being collisions between
  // static strings.
  if (!Equal(it->value, characters, length))
    return nullptr;
  return it->value;
}

String AttemptStaticStringCreation(const LChar* characters, wtf_size_t size) {
  String string(FindStringIfStatic(characters, size));
  if (string.Impl())
    return string;
  return String(characters, size);
}

String AttemptStaticStringCreation(const UChar* characters,
                                   wtf_size_t size,
                                   CharacterWidth width) {
  String string(FindStringIfStatic(characters, size));
  if (string.Impl())
    return string;
  if (width == kLikely8Bit)
    string = StringImpl::Create8BitIfPossible(characters, size);
  else if (width == kForce8Bit)
    string = String::Make8BitFrom16BitSource(characters, size);
  else
    string = String(characters, size);

  return string;
}

}  // namespace blink
