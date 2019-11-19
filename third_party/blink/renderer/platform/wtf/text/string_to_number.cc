// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

#include <type_traits>
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"

namespace WTF {

template <int base>
bool IsCharacterAllowedInBase(UChar);

template <>
bool IsCharacterAllowedInBase<10>(UChar c) {
  return IsASCIIDigit(c);
}

template <>
bool IsCharacterAllowedInBase<16>(UChar c) {
  return IsASCIIHexDigit(c);
}

template <typename IntegralType, typename CharType, int base>
static inline IntegralType ToIntegralType(const CharType* data,
                                          size_t length,
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

  IntegralType value = 0;
  NumberParsingResult result = NumberParsingResult::kError;
  bool is_negative = false;
  bool overflow = false;
  const bool accept_minus = kIsSigned || options.AcceptMinusZeroForUnsigned();

  if (!data)
    goto bye;

  if (options.AcceptWhitespace()) {
    while (length && IsSpaceOrNewline(*data)) {
      --length;
      ++data;
    }
  }

  if (accept_minus && length && *data == '-') {
    --length;
    ++data;
    is_negative = true;
  } else if (length && options.AcceptLeadingPlus() && *data == '+') {
    --length;
    ++data;
  }

  if (!length || !IsCharacterAllowedInBase<base>(*data))
    goto bye;

  while (length && IsCharacterAllowedInBase<base>(*data)) {
    --length;
    IntegralType digit_value;
    CharType c = *data;
    if (IsASCIIDigit(c))
      digit_value = c - '0';
    else if (c >= 'a')
      digit_value = c - 'a' + 10;
    else
      digit_value = c - 'A' + 10;

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
    ++data;
  }

  if (options.AcceptWhitespace()) {
    while (length && IsSpaceOrNewline(*data)) {
      --length;
      ++data;
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

template <typename IntegralType, typename CharType, int base>
static inline IntegralType ToIntegralType(const CharType* data,
                                          size_t length,
                                          NumberParsingOptions options,
                                          bool* ok) {
  NumberParsingResult result;
  IntegralType value = ToIntegralType<IntegralType, CharType, base>(
      data, length, options, &result);
  if (ok)
    *ok = result == NumberParsingResult::kSuccess;
  return value;
}

unsigned CharactersToUInt(const LChar* data,
                          size_t length,
                          NumberParsingOptions options,
                          NumberParsingResult* result) {
  return ToIntegralType<unsigned, LChar, 10>(data, length, options, result);
}

unsigned CharactersToUInt(const UChar* data,
                          size_t length,
                          NumberParsingOptions options,
                          NumberParsingResult* result) {
  return ToIntegralType<unsigned, UChar, 10>(data, length, options, result);
}

unsigned HexCharactersToUInt(const LChar* data,
                             size_t length,
                             NumberParsingOptions options,
                             bool* ok) {
  return ToIntegralType<unsigned, LChar, 16>(data, length, options, ok);
}

unsigned HexCharactersToUInt(const UChar* data,
                             size_t length,
                             NumberParsingOptions options,
                             bool* ok) {
  return ToIntegralType<unsigned, UChar, 16>(data, length, options, ok);
}

int CharactersToInt(const LChar* data,
                    size_t length,
                    NumberParsingOptions options,
                    bool* ok) {
  return ToIntegralType<int, LChar, 10>(data, length, options, ok);
}

int CharactersToInt(const UChar* data,
                    size_t length,
                    NumberParsingOptions options,
                    bool* ok) {
  return ToIntegralType<int, UChar, 10>(data, length, options, ok);
}

unsigned CharactersToUInt(const LChar* data,
                          size_t length,
                          NumberParsingOptions options,
                          bool* ok) {
  return ToIntegralType<unsigned, LChar, 10>(data, length, options, ok);
}

unsigned CharactersToUInt(const UChar* data,
                          size_t length,
                          NumberParsingOptions options,
                          bool* ok) {
  return ToIntegralType<unsigned, UChar, 10>(data, length, options, ok);
}

int64_t CharactersToInt64(const LChar* data,
                          size_t length,
                          NumberParsingOptions options,
                          bool* ok) {
  return ToIntegralType<int64_t, LChar, 10>(data, length, options, ok);
}

int64_t CharactersToInt64(const UChar* data,
                          size_t length,
                          NumberParsingOptions options,
                          bool* ok) {
  return ToIntegralType<int64_t, UChar, 10>(data, length, options, ok);
}

uint64_t CharactersToUInt64(const LChar* data,
                            size_t length,
                            NumberParsingOptions options,
                            bool* ok) {
  return ToIntegralType<uint64_t, LChar, 10>(data, length, options, ok);
}

uint64_t CharactersToUInt64(const UChar* data,
                            size_t length,
                            NumberParsingOptions options,
                            bool* ok) {
  return ToIntegralType<uint64_t, UChar, 10>(data, length, options, ok);
}

enum TrailingJunkPolicy { kDisallowTrailingJunk, kAllowTrailingJunk };

template <typename CharType, TrailingJunkPolicy policy>
static inline double ToDoubleType(const CharType* data,
                                  size_t length,
                                  bool* ok,
                                  size_t& parsed_length) {
  size_t leading_spaces_length = 0;
  while (leading_spaces_length < length &&
         IsASCIISpace(data[leading_spaces_length]))
    ++leading_spaces_length;

  double number = ParseDouble(data + leading_spaces_length,
                              length - leading_spaces_length, parsed_length);
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

double CharactersToDouble(const LChar* data, size_t length, bool* ok) {
  size_t parsed_length;
  return ToDoubleType<LChar, kDisallowTrailingJunk>(data, length, ok,
                                                    parsed_length);
}

double CharactersToDouble(const UChar* data, size_t length, bool* ok) {
  size_t parsed_length;
  return ToDoubleType<UChar, kDisallowTrailingJunk>(data, length, ok,
                                                    parsed_length);
}

double CharactersToDouble(const LChar* data,
                          size_t length,
                          size_t& parsed_length) {
  return ToDoubleType<LChar, kAllowTrailingJunk>(data, length, nullptr,
                                                 parsed_length);
}

double CharactersToDouble(const UChar* data,
                          size_t length,
                          size_t& parsed_length) {
  return ToDoubleType<UChar, kAllowTrailingJunk>(data, length, nullptr,
                                                 parsed_length);
}

float CharactersToFloat(const LChar* data, size_t length, bool* ok) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  size_t parsed_length;
  return static_cast<float>(ToDoubleType<LChar, kDisallowTrailingJunk>(
      data, length, ok, parsed_length));
}

float CharactersToFloat(const UChar* data, size_t length, bool* ok) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  size_t parsed_length;
  return static_cast<float>(ToDoubleType<UChar, kDisallowTrailingJunk>(
      data, length, ok, parsed_length));
}

float CharactersToFloat(const LChar* data,
                        size_t length,
                        size_t& parsed_length) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  return static_cast<float>(ToDoubleType<LChar, kAllowTrailingJunk>(
      data, length, nullptr, parsed_length));
}

float CharactersToFloat(const UChar* data,
                        size_t length,
                        size_t& parsed_length) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  return static_cast<float>(ToDoubleType<UChar, kAllowTrailingJunk>(
      data, length, nullptr, parsed_length));
}

}  // namespace WTF
