// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

#include <type_traits>

#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

template <int base>
bool IsCharacterAllowedInBase(UChar);

template <>
bool IsCharacterAllowedInBase<10>(UChar c) {
  return IsAsciiDigit(c);
}

template <>
bool IsCharacterAllowedInBase<16>(UChar c) {
  return IsAsciiHexDigit(c);
}

template <typename IntegralType, int base, typename CharType>
static inline IntegralType ToIntegralType(base::span<const CharType> chars,
                                          NumberParsingOptions options,
                                          NumberParsingResult* parsing_result) {
  static_assert(std::is_integral<IntegralType>::value,
                "IntegralType must be an integral type.");
  static constexpr IntegralType kIntegralMax =
      std::numeric_limits<IntegralType>::max();
  static constexpr IntegralType kIntegralMin =
      std::numeric_limits<IntegralType>::min();
  static constexpr bool kIsSigned =
      std::numeric_limits<IntegralType>::is_signed;
  DCHECK(parsing_result);

  auto data = chars;
  size_t index = 0;
  size_t length = data.size();
  IntegralType value = 0;
  NumberParsingResult result = NumberParsingResult::kError;
  bool is_negative = false;
  bool overflow = false;
  const bool accept_minus = kIsSigned || options.AcceptMinusZeroForUnsigned();

  if (!data.data()) {
    goto bye;
  }

  if (options.AcceptWhitespace()) {
    while (length && blink::unicode::IsSpaceOrNewline(data[index])) {
      --length;
      ++index;
    }
  }

  if (accept_minus && length && data[index] == '-') {
    --length;
    ++index;
    is_negative = true;
  } else if (length && options.AcceptLeadingPlus() && data[index] == '+') {
    --length;
    ++index;
  }

  if (!length || !IsCharacterAllowedInBase<base>(data[index])) {
    goto bye;
  }

  while (length && IsCharacterAllowedInBase<base>(data[index])) {
    --length;
    IntegralType digit_value;
    CharType c = data[index];
    if (IsAsciiDigit(c)) {
      digit_value = c - '0';
    } else if (c >= 'a') {
      digit_value = c - 'a' + 10;
    } else {
      digit_value = c - 'A' + 10;
    }

    if (is_negative) {
      if (!kIsSigned && options.AcceptMinusZeroForUnsigned()) {
        if (digit_value != 0) {
          result = NumberParsingResult::kError;
          overflow = true;
        }
      } else {
        // Overflow condition:
        //       value * base - digit_value < kIntegralMin
        //   <=> value < (kIntegralMin + digit_value) / base
        // We must be careful of rounding errors here, but the default rounding
        // mode (round to zero) works well, so we can use this formula as-is.
        if (value < (kIntegralMin + digit_value) / base) {
          result = NumberParsingResult::kOverflowMin;
          overflow = true;
        }
      }
    } else {
      // Overflow condition:
      //       value * base + digit_value > kIntegralMax
      //   <=> value > (kIntegralMax + digit_value) / base
      // Ditto regarding rounding errors.
      if (value > (kIntegralMax - digit_value) / base) {
        result = NumberParsingResult::kOverflowMax;
        overflow = true;
      }
    }

    if (!overflow) {
      if (is_negative)
        value = base * value - digit_value;
      else
        value = base * value + digit_value;
    }
    ++index;
  }

  if (options.AcceptWhitespace()) {
    while (length && blink::unicode::IsSpaceOrNewline(data[index])) {
      --length;
      ++index;
    }
  }

  if (length == 0 || options.AcceptTrailingGarbage()) {
    if (!overflow)
      result = NumberParsingResult::kSuccess;
  } else {
    // Even if we detected overflow, we return kError for trailing garbage.
    result = NumberParsingResult::kError;
  }
bye:
  *parsing_result = result;
  return result == NumberParsingResult::kSuccess ? value : 0;
}

template <typename IntegralType, int base, typename CharType>
static inline std::optional<IntegralType> ToIntegralType(
    base::span<const CharType> data,
    NumberParsingOptions options) {
  NumberParsingResult result;
  auto value = ToIntegralType<IntegralType, base>(data, options, &result);
  return result == NumberParsingResult::kSuccess ? std::make_optional(value)
                                                 : std::nullopt;
}

unsigned CharactersToUInt(base::span<const LChar> data,
                          NumberParsingOptions options,
                          NumberParsingResult* result) {
  return ToIntegralType<unsigned, 10>(data, options, result);
}

unsigned CharactersToUInt(base::span<const UChar> data,
                          NumberParsingOptions options,
                          NumberParsingResult* result) {
  return ToIntegralType<unsigned, 10>(data, options, result);
}

std::optional<uint32_t> HexCharactersToUInt(base::span<const LChar> data,
                                            NumberParsingOptions options) {
  return ToIntegralType<uint32_t, 16>(data, options);
}

std::optional<uint32_t> HexCharactersToUInt(base::span<const UChar> data,
                                            NumberParsingOptions options) {
  return ToIntegralType<uint32_t, 16>(data, options);
}

std::optional<uint64_t> HexCharactersToUInt64(base::span<const LChar> data,
                                              NumberParsingOptions options) {
  return ToIntegralType<uint64_t, 16>(data, options);
}

std::optional<uint64_t> HexCharactersToUInt64(base::span<const UChar> data,
                                              NumberParsingOptions options) {
  return ToIntegralType<uint64_t, 16>(data, options);
}

std::optional<int32_t> CharactersToInt(base::span<const LChar> data,
                                       NumberParsingOptions options) {
  return ToIntegralType<int32_t, 10>(data, options);
}

std::optional<int32_t> CharactersToInt(base::span<const UChar> data,
                                       NumberParsingOptions options) {
  return ToIntegralType<int32_t, 10>(data, options);
}

std::optional<uint32_t> CharactersToUInt(base::span<const LChar> data,
                                         NumberParsingOptions options) {
  return ToIntegralType<uint32_t, 10>(data, options);
}

std::optional<uint32_t> CharactersToUInt(base::span<const UChar> data,
                                         NumberParsingOptions options) {
  return ToIntegralType<uint32_t, 10>(data, options);
}

std::optional<int64_t> CharactersToInt64(base::span<const LChar> data,
                                         NumberParsingOptions options) {
  return ToIntegralType<int64_t, 10>(data, options);
}

std::optional<int64_t> CharactersToInt64(base::span<const UChar> data,
                                         NumberParsingOptions options) {
  return ToIntegralType<int64_t, 10>(data, options);
}

std::optional<uint64_t> CharactersToUInt64(base::span<const LChar> data,
                                           NumberParsingOptions options) {
  return ToIntegralType<uint64_t, 10>(data, options);
}

std::optional<uint64_t> CharactersToUInt64(base::span<const UChar> data,
                                           NumberParsingOptions options) {
  return ToIntegralType<uint64_t, 10>(data, options);
}

enum TrailingJunkPolicy { kDisallowTrailingJunk, kAllowTrailingJunk };

template <typename CharType, TrailingJunkPolicy policy>
static inline double ToDoubleType(base::span<const CharType> data,
                                  bool* ok,
                                  size_t& parsed_length) {
  size_t length = data.size();
  size_t leading_spaces_length = 0;
  while (leading_spaces_length < length &&
         IsAsciiSpace(data[leading_spaces_length])) {
    ++leading_spaces_length;
  }

  double number =
      ParseDouble(data.subspan(leading_spaces_length), parsed_length);
  if (!parsed_length) {
    if (ok)
      *ok = false;
    return 0.0;
  }

  parsed_length += leading_spaces_length;
  if (ok)
    *ok = policy == kAllowTrailingJunk || parsed_length == length;
  return number;
}

double CharactersToDouble(base::span<const LChar> data, bool* ok) {
  size_t parsed_length;
  return ToDoubleType<LChar, kDisallowTrailingJunk>(data, ok, parsed_length);
}

double CharactersToDouble(base::span<const UChar> data, bool* ok) {
  size_t parsed_length;
  return ToDoubleType<UChar, kDisallowTrailingJunk>(data, ok, parsed_length);
}

double CharactersToDouble(base::span<const LChar> data, size_t& parsed_length) {
  return ToDoubleType<LChar, kAllowTrailingJunk>(data, nullptr, parsed_length);
}

double CharactersToDouble(base::span<const UChar> data, size_t& parsed_length) {
  return ToDoubleType<UChar, kAllowTrailingJunk>(data, nullptr, parsed_length);
}

float CharactersToFloat(base::span<const LChar> data, bool* ok) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  size_t parsed_length;
  return static_cast<float>(
      ToDoubleType<LChar, kDisallowTrailingJunk>(data, ok, parsed_length));
}

float CharactersToFloat(base::span<const UChar> data, bool* ok) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  size_t parsed_length;
  return static_cast<float>(
      ToDoubleType<UChar, kDisallowTrailingJunk>(data, ok, parsed_length));
}

float CharactersToFloat(base::span<const LChar> data, size_t& parsed_length) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  return static_cast<float>(
      ToDoubleType<LChar, kAllowTrailingJunk>(data, nullptr, parsed_length));
}

float CharactersToFloat(base::span<const UChar> data, size_t& parsed_length) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  return static_cast<float>(
      ToDoubleType<UChar, kAllowTrailingJunk>(data, nullptr, parsed_length));
}

std::optional<int32_t> StringToInt(const StringView& input,
                                   NumberParsingOptions options) {
  return VisitCharacters(
      input, [&](auto chars) { return CharactersToInt(chars, options); });
}

std::optional<uint32_t> StringToUint(const StringView& input,
                                     NumberParsingOptions options) {
  return VisitCharacters(
      input, [&](auto chars) { return CharactersToUInt(chars, options); });
}

std::optional<int64_t> StringToInt64(const StringView& input,
                                     NumberParsingOptions options) {
  return input.Is8Bit() ? CharactersToInt64(input.Span8(), options)
                        : CharactersToInt64(input.Span16(), options);
}

std::optional<uint64_t> StringToUint64(const StringView& input,
                                       NumberParsingOptions options) {
  return input.Is8Bit() ? CharactersToUInt64(input.Span8(), options)
                        : CharactersToUInt64(input.Span16(), options);
}

std::optional<uint32_t> HexStringToUint(const StringView& input,
                                        NumberParsingOptions options) {
  return input.Is8Bit() ? HexCharactersToUInt(input.Span8(), options)
                        : HexCharactersToUInt(input.Span16(), options);
}

std::optional<uint64_t> HexStringToUint64(const StringView& input,
                                          NumberParsingOptions options) {
  return input.Is8Bit() ? HexCharactersToUInt64(input.Span8(), options)
                        : HexCharactersToUInt64(input.Span16(), options);
}

std::optional<int32_t> StringToIntStrict(const StringView& input) {
  constexpr NumberParsingOptions kOption = NumberParsingOptions::Strict();
  return input.Is8Bit() ? CharactersToInt(input.Span8(), kOption)
                        : CharactersToInt(input.Span16(), kOption);
}

std::optional<uint32_t> StringToUintStrict(const StringView& input) {
  constexpr NumberParsingOptions kOption = NumberParsingOptions::Strict();
  return input.Is8Bit() ? CharactersToUInt(input.Span8(), kOption)
                        : CharactersToUInt(input.Span16(), kOption);
}

std::optional<int32_t> StringToIntLoose(const StringView& input) {
  return StringToInt(input, NumberParsingOptions::Loose());
}

std::optional<uint32_t> StringToUintLoose(const StringView& input) {
  return StringToUint(input, NumberParsingOptions::Loose());
}

std::optional<double> StringToDouble(const StringView& input) {
  bool ok = false;
  double value = input.Is8Bit() ? CharactersToDouble(input.Span8(), &ok)
                                : CharactersToDouble(input.Span16(), &ok);
  return ok ? std::optional<double>(value) : std::nullopt;
}

std::optional<float> StringToFloat(const StringView& input) {
  bool ok = false;
  float value = input.Is8Bit() ? CharactersToFloat(input.Span8(), &ok)
                               : CharactersToFloat(input.Span16(), &ok);
  return ok ? std::optional<float>(value) : std::nullopt;
}

}  // namespace blink
