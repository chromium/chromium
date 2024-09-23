// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2010 Daniel Bates (dbates@intudata.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/counter_style.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// User agents must support representations at least 60 Unicode codepoints long,
// but they may choose to instead use the fallback style for representations
// that would be longer than 60 codepoints. Since WTF::String may use UTF-16, we
// limit string length at 120.
const wtf_size_t kCounterLengthLimit = 120;

const CounterStyle& GetDisc() {
  const CounterStyle* disc =
      CounterStyleMap::GetUACounterStyleMap()->FindCounterStyleAcrossScopes(
          keywords::kDisc);
  DCHECK(disc);
  return *disc;
}

bool HasSymbols(CounterStyleSystem system) {
  switch (system) {
    case CounterStyleSystem::kCyclic:
    case CounterStyleSystem::kFixed:
    case CounterStyleSystem::kSymbolic:
    case CounterStyleSystem::kAlphabetic:
    case CounterStyleSystem::kNumeric:
    case CounterStyleSystem::kAdditive:
      return true;
    case CounterStyleSystem::kUnresolvedExtends:
    case CounterStyleSystem::kHebrew:
    case CounterStyleSystem::kSimpChineseInformal:
    case CounterStyleSystem::kSimpChineseFormal:
    case CounterStyleSystem::kTradChineseInformal:
    case CounterStyleSystem::kTradChineseFormal:
    case CounterStyleSystem::kKoreanHangulFormal:
    case CounterStyleSystem::kKoreanHanjaInformal:
    case CounterStyleSystem::kKoreanHanjaFormal:
    case CounterStyleSystem::kLowerArmenian:
    case CounterStyleSystem::kUpperArmenian:
    case CounterStyleSystem::kEthiopicNumeric:
      return false;
  }
}

String SymbolToString(const CSSValue& value) {
  if (const CSSStringValue* string = DynamicTo<CSSStringValue>(value)) {
    return string->Value();
  }
  return To<CSSCustomIdentValue>(value).Value();
}

std::pair<int, int> BoundsToIntegerPair(const CSSValuePair& bounds) {
  int lower_bound, upper_bound;
  if (bounds.First().IsIdentifierValue()) {
    DCHECK_EQ(CSSValueID::kInfinite,
              To<CSSIdentifierValue>(bounds.First()).GetValueID());
    lower_bound = std::numeric_limits<int>::min();
  } else {
    DCHECK(bounds.First().IsPrimitiveValue());
    lower_bound = To<CSSPrimitiveValue>(bounds.First()).GetIntValue();
  }
  if (bounds.Second().IsIdentifierValue()) {
    DCHECK_EQ(CSSValueID::kInfinite,
              To<CSSIdentifierValue>(bounds.Second()).GetValueID());
    upper_bound = std::numeric_limits<int>::max();
  } else {
    DCHECK(bounds.Second().IsPrimitiveValue());
    upper_bound = To<CSSPrimitiveValue>(bounds.Second()).GetIntValue();
  }
  return std::make_pair(lower_bound, upper_bound);
}

// https://drafts.csswg.org/css-counter-styles/#cyclic-system
Vector<wtf_size_t> CyclicAlgorithm(int value, wtf_size_t num_symbols) {
  DCHECK(num_symbols);
  value %= static_cast<int>(num_symbols);
  value -= 1;
  if (value < 0) {
    value += num_symbols;
  }
  return {static_cast<wtf_size_t>(value)};
}

// https://drafts.csswg.org/css-counter-styles/#fixed-system
Vector<wtf_size_t> FixedAlgorithm(int value,
                                  int first_symbol_value,
                                  wtf_size_t num_symbols) {
  if (value < first_symbol_value ||
      static_cast<unsigned>(value - first_symbol_value) >= num_symbols) {
    return Vector<wtf_size_t>();
  }
  return {static_cast<wtf_size_t>(value - first_symbol_value)};
}

// https://drafts.csswg.org/css-counter-styles/#symbolic-system
Vector<wtf_size_t> SymbolicAlgorithm(unsigned value, wtf_size_t num_symbols) {
  DCHECK(num_symbols);
  if (!value) {
    return Vector<wtf_size_t>();
  }
  wtf_size_t index = (value - 1) % num_symbols;
  wtf_size_t repetitions = (value + num_symbols - 1) / num_symbols;
  if (repetitions > kCounterLengthLimit) {
    return Vector<wtf_size_t>();
  }
  return Vector<wtf_size_t>(repetitions, index);
}

// https://drafts.csswg.org/css-counter-styles/#alphabetic-system
Vector<wtf_size_t> AlphabeticAlgorithm(unsigned value, wtf_size_t num_symbols) {
  DCHECK(num_symbols);
  if (!value) {
    return Vector<wtf_size_t>();
  }
  Vector<wtf_size_t> result;
  while (value) {
    value -= 1;
    result.push_back(value % num_symbols);
    value /= num_symbols;

    // Since length is logarithmic to value, we won't exceed the length limit.
    DCHECK_LE(result.size(), kCounterLengthLimit);
  }
  std::reverse(result.begin(), result.end());
  return result;
}

// https://drafts.csswg.org/css-counter-styles/#numeric-system
Vector<wtf_size_t> NumericAlgorithm(unsigned value, wtf_size_t num_symbols) {
  DCHECK_GT(num_symbols, 1u);
  if (!value) {
    return {0};
  }

  Vector<wtf_size_t> result;
  while (value) {
    result.push_back(value % num_symbols);
    value /= num_symbols;

    // Since length is logarithmic to value, we won't exceed the length limit.
    DCHECK_LE(result.size(), kCounterLengthLimit);
  }
  std::reverse(result.begin(), result.end());
  return result;
}

// https://drafts.csswg.org/css-counter-styles/#additive-system
Vector<wtf_size_t> AdditiveAlgorithm(unsigned value,
                                     const Vector<unsigned>& weights) {
  DCHECK(weights.size());
  if (!value) {
    if (weights.back() == 0u) {
      return {weights.size() - 1};
    }
    return Vector<wtf_size_t>();
  }

  Vector<wtf_size_t> result;
  for (wtf_size_t index = 0; value && index < weights.size() && weights[index];
       ++index) {
    wtf_size_t repetitions = value / weights[index];
    if (repetitions) {
      if (result.size() + repetitions > kCounterLengthLimit) {
        return Vector<wtf_size_t>();
      }
      result.AppendVector(Vector<wtf_size_t>(repetitions, index));
    }
    value %= weights[index];
  }
  if (value) {
    return Vector<wtf_size_t>();
  }
  return result;
}

enum CJKLang { kChinese = 1, kKorean, kJapanese };

enum CJKStyle { kFormal, kInformal };

// The table uses the order from the CSS3 specification:
// first 3 group markers, then 3 digit markers, then ten digits.
String CJKIdeoGraphicAlgorithm(unsigned number,
                               const UChar table[21],
                               CJKStyle cjk_style) {
  enum AbstractCJKChar {
    kNoChar = 0,
    kLang = 0,
    // FourthGroupMarker for simplified chinese has two codepoints, to simplify
    // the main algorithm below use two codepoints for all group markers.
    kSecondGroupMarker = 1,
    kThirdGroupMarker = 3,
    kFourthGroupMarker = 5,
    kSecondDigitMarker = 7,
    kThirdDigitMarker,
    kFourthDigitMarker,
    kDigit0,
    kDigit1,
    kDigit2,
    kDigit3,
    kDigit4,
    kDigit5,
    kDigit6,
    kDigit7,
    kDigit8,
    kDigit9
  };

  if (number == 0) {
    return String(&table[kDigit0], 1u);
  }

  const unsigned kGroupLength =
      9;  // 4 digits, 3 digit markers, group marker of size 2.
  const unsigned kBufferLength = 4 * kGroupLength;
  AbstractCJKChar buffer[kBufferLength] = {kNoChar};

  for (unsigned i = 0; i < 4; ++i) {
    unsigned group_value = number % 10000;
    number /= 10000;

    // Process least-significant group first, but put it in the buffer last.
    AbstractCJKChar* group = &buffer[(3 - i) * kGroupLength];

    if (group_value && i) {
      group[8] = static_cast<AbstractCJKChar>(kSecondGroupMarker + i);
      group[7] = static_cast<AbstractCJKChar>(kSecondGroupMarker - 1 + i);
    }

    // Put in the four digits and digit markers for any non-zero digits.
    unsigned digit_value = (group_value % 10);
    bool trailing_zero = table[kLang] == kChinese && !digit_value;
    if (digit_value) {
      bool drop_one = table[kLang] == kKorean && cjk_style == kInformal &&
                      digit_value == 1 && i > 0;
      if (!drop_one) {
        group[6] = static_cast<AbstractCJKChar>(kDigit0 + (group_value % 10));
      }
    }
    if (number != 0 || group_value > 9) {
      digit_value = ((group_value / 10) % 10);
      bool drop_one =
          table[kLang] == kKorean && cjk_style == kInformal && digit_value == 1;
      if ((digit_value && !drop_one) || (!digit_value && !trailing_zero)) {
        group[4] = static_cast<AbstractCJKChar>(kDigit0 + digit_value);
      }
      trailing_zero &= !digit_value;
      if (digit_value) {
        group[5] = kSecondDigitMarker;
      }
    }
    if (number != 0 || group_value > 99) {
      digit_value = ((group_value / 100) % 10);
      bool drop_one =
          table[kLang] == kKorean && cjk_style == kInformal && digit_value == 1;
      if ((digit_value && !drop_one) || (!digit_value && !trailing_zero)) {
        group[2] = static_cast<AbstractCJKChar>(kDigit0 + digit_value);
      }
      trailing_zero &= !digit_value;
      if (digit_value) {
        group[3] = kThirdDigitMarker;
      }
    }
    if (number != 0 || group_value > 999) {
      digit_value = group_value / 1000;
      bool drop_one =
          table[kLang] == kKorean && cjk_style == kInformal && digit_value == 1;
      if ((digit_value && !drop_one) || (!digit_value && !trailing_zero)) {
        group[0] = static_cast<AbstractCJKChar>(kDigit0 + digit_value);
      }
      if (digit_value) {
        group[1] = kFourthDigitMarker;
      }
    }

    if (trailing_zero && i > 0) {
      group[6] = group[7];
      group[7] = group[8];
      group[8] = kDigit0;
    }

    // Remove the tens digit, but leave the marker, for any group that has
    // a value of less than 20.
    if (table[kLang] == kChinese && cjk_style == kInformal &&
        group_value < 20) {
      DCHECK(group[4] == kNoChar || group[4] == kDigit0 || group[4] == kDigit1);
      group[4] = kNoChar;
    }

    if (number == 0) {
      break;
    }
  }

  // Convert into characters, omitting consecutive runs of Digit0 and
  // any trailing Digit0.
  unsigned length = 0;
  UChar characters[kBufferLength];
  AbstractCJKChar last = kNoChar;
  for (unsigned i = 0; i < kBufferLength; ++i) {
    AbstractCJKChar a = buffer[i];
    if (a != kNoChar) {
      if (a != kDigit0 || (table[kLang] == kChinese && last != kDigit0)) {
        UChar new_char = table[a];
        if (new_char != kNoChar) {
          characters[length++] = table[a];
          if (table[kLang] == kKorean &&
              (a == kSecondGroupMarker || a == kThirdGroupMarker ||
               a == kFourthGroupMarker)) {
            characters[length++] = ' ';
          }
        }
      }
      last = a;
    }
  }
  if ((table[kLang] == kChinese && last == kDigit0) ||
      characters[length - 1] == ' ') {
    --length;
  }

  return String(characters, length);
}

String SimpChineseInformalAlgorithm(unsigned value) {
  static const UChar kSimpleChineseInformalTable[21] = {
      kChinese, 0x4E07, 0x0000, 0x4EBF, 0x0000, 0x4E07, 0x4EBF,
      0x5341,   0x767E, 0x5343, 0x96F6, 0x4E00, 0x4E8C, 0x4E09,
      0x56DB,   0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D, 0x0000};
  return CJKIdeoGraphicAlgorithm(value, kSimpleChineseInformalTable, kInformal);
}

String SimpChineseFormalAlgorithm(unsigned value) {
  static const UChar kSimpleChineseFormalTable[21] = {
      kChinese, 0x4E07, 0x0000, 0x4EBF, 0x0000, 0x4E07, 0x4EBF,
      0x62FE,   0x4F70, 0x4EDF, 0x96F6, 0x58F9, 0x8D30, 0x53C1,
      0x8086,   0x4F0D, 0x9646, 0x67D2, 0x634C, 0x7396, 0x0000};
  return CJKIdeoGraphicAlgorithm(value, kSimpleChineseFormalTable, kFormal);
}

String TradChineseInformalAlgorithm(unsigned value) {
  static const UChar kTraditionalChineseInformalTable[21] = {
      kChinese, 0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000,
      0x5341,   0x767E, 0x5343, 0x96F6, 0x4E00, 0x4E8C, 0x4E09,
      0x56DB,   0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D, 0x0000};
  return CJKIdeoGraphicAlgorithm(value, kTraditionalChineseInformalTable,
                                 kInformal);
}

String TradChineseFormalAlgorithm(unsigned value) {
  static const UChar kTraditionalChineseFormalTable[21] = {
      kChinese, 0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000,
      0x62FE,   0x4F70, 0x4EDF, 0x96F6, 0x58F9, 0x8CB3, 0x53C3,
      0x8086,   0x4F0D, 0x9678, 0x67D2, 0x634C, 0x7396, 0x0000};
  return CJKIdeoGraphicAlgorithm(value, kTraditionalChineseFormalTable,
                                 kFormal);
}

String KoreanHangulFormalAlgorithm(unsigned value) {
  static const UChar kKoreanHangulFormalTable[21] = {
      kKorean, 0xB9CC, 0x0000, 0xC5B5, 0x0000, 0xC870, 0x0000,
      0xC2ED,  0xBC31, 0xCC9C, 0xC601, 0xC77C, 0xC774, 0xC0BC,
      0xC0AC,  0xC624, 0xC721, 0xCE60, 0xD314, 0xAD6C, 0x0000};
  return CJKIdeoGraphicAlgorithm(value, kKoreanHangulFormalTable, kFormal);
}

String KoreanHanjaInformalAlgorithm(unsigned value) {
  static const UChar kKoreanHanjaInformalTable[21] = {
      kKorean, 0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000,
      0x5341,  0x767E, 0x5343, 0x96F6, 0x4E00, 0x4E8C, 0x4E09,
      0x56DB,  0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D, 0x0000};
  return CJKIdeoGraphicAlgorithm(value, kKoreanHanjaInformalTable, kInformal);
}

String KoreanHanjaFormalAlgorithm(unsigned value) {
  static const UChar kKoreanHanjaFormalTable[21] = {
      kKorean, 0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000,
      0x62FE,  0x767E, 0x4EDF, 0x96F6, 0x58F9, 0x8CB3, 0x53C3,
      0x56DB,  0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D, 0x0000};
  return CJKIdeoGraphicAlgorithm(value, kKoreanHanjaFormalTable, kFormal);
}

String HebrewAlgorithmUnder1000(unsigned number) {
  // FIXME: CSS3 mentions various refinements not implemented here.
  // FIXME: Should take a look at Mozilla's HebrewToText function (in
  // CounterStyleManager.cpp).
  DCHECK_LT(number, 1000u);
  StringBuilder letters;
  unsigned four_hundreds = number / 400;
  for (unsigned i = 0; i < four_hundreds; i++) {
    letters.Append(static_cast<UChar>(1511 + 3));
  }
  number %= 400;
  if (number / 100) {
    letters.Append(static_cast<UChar>(1511 + (number / 100) - 1));
  }
  number %= 100;
  if (number == 15 || number == 16) {
    letters.Append(static_cast<UChar>(1487 + 9));
    letters.Append(static_cast<UChar>(1487 + number - 9));
  } else {
    if (unsigned tens = number / 10) {
      static const UChar kHebrewTens[9] = {1497, 1499, 1500, 1502, 1504,
                                           1505, 1506, 1508, 1510};
      letters.Append(kHebrewTens[tens - 1]);
    }
    if (unsigned ones = number % 10) {
      letters.Append(static_cast<UChar>(1487 + ones));
    }
  }
  return letters.ReleaseString();
}

String HebrewAlgorithm(unsigned number) {
  // FIXME: CSS3 mentions ways to make this work for much larger numbers.
  if (number > 999999) {
    return String();
  }

  if (number == 0) {
    static const UChar kHebrewZero[3] = {0x05D0, 0x05E4, 0x05E1};
    return String(kHebrewZero, 3u);
  }

  if (number <= 999) {
    return HebrewAlgorithmUnder1000(number);
  }

  return HebrewAlgorithmUnder1000(number / 1000) +
         kHebrewPunctuationGereshCharacter +
         HebrewAlgorithmUnder1000(number % 1000);
}

String ArmenianAlgorithmUnder10000(unsigned number,
                                   bool upper,
                                   bool add_circumflex) {
  DCHECK_LT(number, 10000u);
  StringBuilder letters;

  unsigned lower_offset = upper ? 0 : 0x0030;

  if (unsigned thousands = number / 1000) {
    if (thousands == 7) {
      letters.Append(static_cast<UChar>(0x0552 + lower_offset));
      if (add_circumflex) {
        letters.Append(static_cast<UChar>(0x0302));
      }
    } else {
      letters.Append(
          static_cast<UChar>((0x054C - 1 + lower_offset) + thousands));
      if (add_circumflex) {
        letters.Append(static_cast<UChar>(0x0302));
      }
    }
  }

  if (unsigned hundreds = (number / 100) % 10) {
    letters.Append(static_cast<UChar>((0x0543 - 1 + lower_offset) + hundreds));
    if (add_circumflex) {
      letters.Append(static_cast<UChar>(0x0302));
    }
  }

  if (unsigned tens = (number / 10) % 10) {
    letters.Append(static_cast<UChar>((0x053A - 1 + lower_offset) + tens));
    if (add_circumflex) {
      letters.Append(static_cast<UChar>(0x0302));
    }
  }

  if (unsigned ones = number % 10) {
    letters.Append(static_cast<UChar>((0x531 - 1 + lower_offset) + ones));
    if (add_circumflex) {
      letters.Append(static_cast<UChar>(0x0302));
    }
  }

  return letters.ReleaseString();
}

String ArmenianAlgorithm(unsigned number, bool upper) {
  if (!number || number > 99999999) {
    return String();
  }
  return ArmenianAlgorithmUnder10000(number / 10000, upper, true) +
         ArmenianAlgorithmUnder10000(number % 10000, upper, false);
}

// https://drafts.csswg.org/css-counter-styles-3/#ethiopic-numeric-counter-style
String EthiopicNumericAlgorithm(unsigned value) {
  // Ethiopic characters for 1-9
  static const UChar units[9] = {0x1369, 0x136A, 0x136B, 0x136C, 0x136D,
                                 0x136E, 0x136F, 0x1370, 0x1371};
  // Ethiopic characters for 10, 20, ..., 90
  static const UChar tens[9] = {0x1372, 0x1373, 0x1374, 0x1375, 0x1376,
                                0x1377, 0x1378, 0x1379, 0x137A};
  if (!value) {
    return String();
  }
  if (value < 10u) {
    return String(&units[value - 1], 1u);
  }

  // Generate characters in the reversed ordering
  Vector<UChar> result;
  for (bool odd_group = false; value; odd_group = !odd_group) {
    unsigned group_value = value % 100;
    value /= 100;
    if (!odd_group) {
      // This adds an extra character for group 0. We'll remove it in the end.
      result.push_back(kEthiopicNumberTenThousandCharacter);
    } else {
      if (group_value) {
        result.push_back(kEthiopicNumberHundredCharacter);
      }
    }
    bool most_significant_group = !value;
    bool remove_digits = !group_value ||
                         (group_value == 1 && most_significant_group) ||
                         (group_value == 1 && odd_group);
    if (!remove_digits) {
      if (unsigned unit = group_value % 10) {
        result.push_back(units[unit - 1]);
      }
      if (unsigned ten = group_value / 10) {
        result.push_back(tens[ten - 1]);
      }
    }
  }

  std::reverse(result.begin(), result.end());
  // Remove the extra character from group 0
  result.pop_back();
  return String(result.data(), result.size());
}

}  // namespace

// static
CounterStyle& CounterStyle::GetDecimal() {
  DEFINE_STATIC_LOCAL(
      Persistent<CounterStyle>, decimal,
      (CounterStyleMap::GetUACounterStyleMap()->FindCounterStyleAcrossScopes(
          keywords::kDecimal)));
  DCHECK(decimal);
  return *decimal;
}

// static
CounterStyleSystem CounterStyle::ToCounterStyleSystemEnum(
    const CSSValue* value) {
  if (!value) {
    return CounterStyleSystem::kSymbolic;
  }

  CSSValueID system_keyword;
  if (const auto* id = DynamicTo<CSSIdentifierValue>(value)) {
    system_keyword = id->GetValueID();
  } else {
    // Either fixed or extends.
    DCHECK(value->IsValuePair());
    const CSSValuePair* pair = To<CSSValuePair>(value);
    DCHECK(pair->First().IsIdentifierValue());
    system_keyword = To<CSSIdentifierValue>(pair->First()).GetValueID();
  }

  switch (system_keyword) {
    case CSSValueID::kCyclic:
      return CounterStyleSystem::kCyclic;
    case CSSValueID::kFixed:
      return CounterStyleSystem::kFixed;
    case CSSValueID::kSymbolic:
      return CounterStyleSystem::kSymbolic;
    case CSSValueID::kAlphabetic:
      return CounterStyleSystem::kAlphabetic;
    case CSSValueID::kNumeric:
      return CounterStyleSystem::kNumeric;
    case CSSValueID::kAdditive:
      return CounterStyleSystem::kAdditive;
    case CSSValueID::kInternalHebrew:
      return CounterStyleSystem::kHebrew;
    case CSSValueID::kInternalSimpChineseInformal:
      return CounterStyleSystem::kSimpChineseInformal;
    case CSSValueID::kInternalSimpChineseFormal:
      return CounterStyleSystem::kSimpChineseFormal;
    case CSSValueID::kInternalTradChineseInformal:
      return CounterStyleSystem::kTradChineseInformal;
    case CSSValueID::kInternalTradChineseFormal:
      return CounterStyleSystem::kTradChineseFormal;
    case CSSValueID::kInternalKoreanHangulFormal:
      return CounterStyleSystem::kKoreanHangulFormal;
    case CSSValueID::kInternalKoreanHanjaInformal:
      return CounterStyleSystem::kKoreanHanjaInformal;
    case CSSValueID::kInternalKoreanHanjaFormal:
      return CounterStyleSystem::kKoreanHanjaFormal;
    case CSSValueID::kInternalLowerArmenian:
      return CounterStyleSystem::kLowerArmenian;
    case CSSValueID::kInternalUpperArmenian:
      return CounterStyleSystem::kUpperArmenian;
    case CSSValueID::kInternalEthiopicNumeric:
      return CounterStyleSystem::kEthiopicNumeric;
    case CSSValueID::kExtends:
      return CounterStyleSystem::kUnresolvedExtends;
    default:
      NOTREACHED_IN_MIGRATION();
      return CounterStyleSystem::kSymbolic;
  }
}

// static
CounterStyleSpeakAs ToCounterStyleSpeakAsEnum(
    const CSSIdentifierValue& keyword) {
  switch (keyword.GetValueID()) {
    case CSSValueID::kAuto:
      return CounterStyleSpeakAs::kAuto;
    case CSSValueID::kBullets:
      return CounterStyleSpeakAs::kBullets;
    case CSSValueID::kNumbers:
      return CounterStyleSpeakAs::kNumbers;
    case CSSValueID::kWords:
      return CounterStyleSpeakAs::kWords;
    default:
      NOTREACHED_IN_MIGRATION();
      return CounterStyleSpeakAs::kAuto;
  }
}

CounterStyle::~CounterStyle() = default;

AtomicString CounterStyle::GetName() const {
  return style_rule_->GetName();
}

// static
CounterStyle* CounterStyle::Create(const StyleRuleCounterStyle& rule) {
  if (!rule.HasValidSymbols()) {
    return nullptr;
  }

  return MakeGarbageCollected<CounterStyle>(rule);
}

CounterStyle::CounterStyle(const StyleRuleCounterStyle& rule)
    : style_rule_(rule), style_rule_version_(rule.GetVersion()) {
  if (const CSSValue* system = rule.GetSystem()) {
    system_ = ToCounterStyleSystemEnum(system);

    if (system_ == CounterStyleSystem::kUnresolvedExtends) {
      const auto& second = To<CSSValuePair>(system)->Second();
      extends_name_ = To<CSSCustomIdentValue>(second).Value();
    } else if (system_ == CounterStyleSystem::kFixed && system->IsValuePair()) {
      const auto& second = To<CSSValuePair>(system)->Second();
      first_symbol_value_ = To<CSSPrimitiveValue>(second).GetIntValue();
    }
  }

  if (const CSSValue* fallback = rule.GetFallback()) {
    fallback_name_ = To<CSSCustomIdentValue>(fallback)->Value();
  }

  if (HasSymbols(system_)) {
    if (system_ == CounterStyleSystem::kAdditive) {
      for (const auto& symbol : To<CSSValueList>(*rule.GetAdditiveSymbols())) {
        const auto& pair = To<CSSValuePair>(*symbol.Get());
        additive_weights_.push_back(
            To<CSSPrimitiveValue>(pair.First()).GetIntValue());
        symbols_.push_back(SymbolToString(pair.Second()));
      }
    } else {
      for (const auto& symbol : To<CSSValueList>(*rule.GetSymbols())) {
        symbols_.push_back(SymbolToString(*symbol.Get()));
      }
    }
  }

  if (const CSSValue* negative = rule.GetNegative()) {
    if (const CSSValuePair* pair = DynamicTo<CSSValuePair>(negative)) {
      negative_prefix_ = SymbolToString(pair->First());
      negative_suffix_ = SymbolToString(pair->Second());
    } else {
      negative_prefix_ = SymbolToString(*negative);
    }
  }

  if (const CSSValue* pad = rule.GetPad()) {
    const CSSValuePair& pair = To<CSSValuePair>(*pad);
    pad_length_ = To<CSSPrimitiveValue>(pair.First()).GetIntValue();
    pad_symbol_ = SymbolToString(pair.Second());
  }

  if (const CSSValue* range = rule.GetRange()) {
    if (range->IsIdentifierValue()) {
      DCHECK_EQ(CSSValueID::kAuto, To<CSSIdentifierValue>(range)->GetValueID());
      // Empty |range_| already means 'auto'.
    } else {
      for (const CSSValue* bounds : To<CSSValueList>(*range)) {
        range_.push_back(BoundsToIntegerPair(To<CSSValuePair>(*bounds)));
      }
    }
  }

  if (const CSSValue* prefix = rule.GetPrefix()) {
    prefix_ = SymbolToString(*prefix);
  }
  if (const CSSValue* suffix = rule.GetSuffix()) {
    suffix_ = SymbolToString(*suffix);
  }

  if (RuntimeEnabledFeatures::CSSAtRuleCounterStyleSpeakAsDescriptorEnabled()) {
    if (const CSSValue* speak_as = rule.GetSpeakAs()) {
      if (const auto* keyword = DynamicTo<CSSIdentifierValue>(speak_as)) {
        speak_as_ = ToCounterStyleSpeakAsEnum(*keyword);
      } else {
        DCHECK(speak_as->IsCustomIdentValue());
        speak_as_ = CounterStyleSpeakAs::kReference;
        speak_as_name_ = To<CSSCustomIdentValue>(speak_as)->Value();
      }
    }
  }
}

void CounterStyle::ResolveExtends(CounterStyle& extended) {
  DCHECK_NE(extended.system_, CounterStyleSystem::kUnresolvedExtends);
  extended_style_ = extended;

  system_ = extended.system_;

  if (system_ == CounterStyleSystem::kFixed) {
    first_symbol_value_ = extended.first_symbol_value_;
  }

  if (!style_rule_->GetFallback()) {
    fallback_name_ = extended.fallback_name_;
    fallback_style_ = nullptr;
  }

  symbols_ = extended.symbols_;
  if (system_ == CounterStyleSystem::kAdditive) {
    additive_weights_ = extended.additive_weights_;
  }

  if (!style_rule_->GetNegative()) {
    negative_prefix_ = extended.negative_prefix_;
    negative_suffix_ = extended.negative_suffix_;
  }

  if (!style_rule_->GetPad()) {
    pad_length_ = extended.pad_length_;
    pad_symbol_ = extended.pad_symbol_;
  }

  if (!style_rule_->GetRange()) {
    range_ = extended.range_;
  }

  if (!style_rule_->GetPrefix()) {
    prefix_ = extended.prefix_;
  }
  if (!style_rule_->GetSuffix()) {
    suffix_ = extended.suffix_;
  }

  if (RuntimeEnabledFeatures::CSSAtRuleCounterStyleSpeakAsDescriptorEnabled()) {
    if (!style_rule_->GetSpeakAs()) {
      speak_as_ = extended.speak_as_;
      speak_as_name_ = extended.speak_as_name_;
      speak_as_style_ = nullptr;
    }
  }
}

bool CounterStyle::RangeContains(int value) const {
  if (range_.size()) {
    for (const auto& bounds : range_) {
      if (value >= bounds.first && value <= bounds.second) {
        return true;
      }
    }
    return false;
  }

  // 'range' value is auto
  switch (system_) {
    case CounterStyleSystem::kCyclic:
    case CounterStyleSystem::kNumeric:
    case CounterStyleSystem::kFixed:
    case CounterStyleSystem::kSimpChineseInformal:
    case CounterStyleSystem::kSimpChineseFormal:
    case CounterStyleSystem::kTradChineseInformal:
    case CounterStyleSystem::kTradChineseFormal:
    case CounterStyleSystem::kKoreanHangulFormal:
    case CounterStyleSystem::kKoreanHanjaInformal:
    case CounterStyleSystem::kKoreanHanjaFormal:
      return true;
    case CounterStyleSystem::kSymbolic:
    case CounterStyleSystem::kAlphabetic:
    case CounterStyleSystem::kEthiopicNumeric:
      return value >= 1;
    case CounterStyleSystem::kAdditive:
      return value >= 0;
    case CounterStyleSystem::kHebrew:
      return value >= 0 && value <= 999999;
    case CounterStyleSystem::kLowerArmenian:
    case CounterStyleSystem::kUpperArmenian:
      return value >= 1 && value <= 99999999;
    case CounterStyleSystem::kUnresolvedExtends:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool CounterStyle::NeedsNegativeSign(int value) const {
  if (value >= 0) {
    return false;
  }
  switch (system_) {
    case CounterStyleSystem::kSymbolic:
    case CounterStyleSystem::kAlphabetic:
    case CounterStyleSystem::kNumeric:
    case CounterStyleSystem::kAdditive:
    case CounterStyleSystem::kHebrew:
    case CounterStyleSystem::kSimpChineseInformal:
    case CounterStyleSystem::kSimpChineseFormal:
    case CounterStyleSystem::kTradChineseInformal:
    case CounterStyleSystem::kTradChineseFormal:
    case CounterStyleSystem::kKoreanHangulFormal:
    case CounterStyleSystem::kKoreanHanjaInformal:
    case CounterStyleSystem::kKoreanHanjaFormal:
    case CounterStyleSystem::kLowerArmenian:
    case CounterStyleSystem::kUpperArmenian:
    case CounterStyleSystem::kEthiopicNumeric:
      return true;
    case CounterStyleSystem::kCyclic:
    case CounterStyleSystem::kFixed:
      return false;
    case CounterStyleSystem::kUnresolvedExtends:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

String CounterStyle::GenerateFallbackRepresentation(int value) const {
  if (is_in_fallback_) {
    // We are in a fallback cycle. Use decimal instead.
    return GetDecimal().GenerateRepresentation(value);
  }

  base::AutoReset<bool> in_fallback_scope(&is_in_fallback_, true);
  return fallback_style_->GenerateRepresentation(value);
}

String CounterStyle::GenerateRepresentation(int value) const {
  DCHECK(!IsDirty());

  if (pad_length_ > kCounterLengthLimit) {
    return GenerateFallbackRepresentation(value);
  }

  String initial_representation = GenerateInitialRepresentation(value);
  if (initial_representation.IsNull()) {
    return GenerateFallbackRepresentation(value);
  }

  wtf_size_t initial_length = NumGraphemeClusters(initial_representation);

  if (NeedsNegativeSign(value)) {
    initial_length += NumGraphemeClusters(negative_prefix_);
    initial_length += NumGraphemeClusters(negative_suffix_);
  }

  wtf_size_t pad_copies =
      pad_length_ > initial_length ? pad_length_ - initial_length : 0;

  StringBuilder result;
  if (NeedsNegativeSign(value)) {
    result.Append(negative_prefix_);
  }
  for (wtf_size_t i = 0; i < pad_copies; ++i) {
    result.Append(pad_symbol_);
  }
  result.Append(initial_representation);
  if (NeedsNegativeSign(value)) {
    result.Append(negative_suffix_);
  }
  return result.ReleaseString();
}

String CounterStyle::GenerateInitialRepresentation(int value) const {
  if (!RangeContains(value)) {
    return String();
  }

  unsigned abs_value =
      value == std::numeric_limits<int>::min()
          ? static_cast<unsigned>(std::numeric_limits<int>::max()) + 1u
          : std::abs(value);

  switch (system_) {
    case CounterStyleSystem::kCyclic:
      return IndexesToString(CyclicAlgorithm(value, symbols_.size()));
    case CounterStyleSystem::kFixed:
      return IndexesToString(
          FixedAlgorithm(value, first_symbol_value_, symbols_.size()));
    case CounterStyleSystem::kNumeric:
      return IndexesToString(NumericAlgorithm(abs_value, symbols_.size()));
    case CounterStyleSystem::kSymbolic:
      return IndexesToString(SymbolicAlgorithm(abs_value, symbols_.size()));
    case CounterStyleSystem::kAlphabetic:
      return IndexesToString(AlphabeticAlgorithm(abs_value, symbols_.size()));
    case CounterStyleSystem::kAdditive:
      return IndexesToString(AdditiveAlgorithm(abs_value, additive_weights_));
    case CounterStyleSystem::kHebrew:
      return HebrewAlgorithm(abs_value);
    case CounterStyleSystem::kSimpChineseInformal:
      return SimpChineseInformalAlgorithm(abs_value);
    case CounterStyleSystem::kSimpChineseFormal:
      return SimpChineseFormalAlgorithm(abs_value);
    case CounterStyleSystem::kTradChineseInformal:
      return TradChineseInformalAlgorithm(abs_value);
    case CounterStyleSystem::kTradChineseFormal:
      return TradChineseFormalAlgorithm(abs_value);
    case CounterStyleSystem::kKoreanHangulFormal:
      return KoreanHangulFormalAlgorithm(abs_value);
    case CounterStyleSystem::kKoreanHanjaInformal:
      return KoreanHanjaInformalAlgorithm(abs_value);
    case CounterStyleSystem::kKoreanHanjaFormal:
      return KoreanHanjaFormalAlgorithm(abs_value);
    case CounterStyleSystem::kLowerArmenian: {
      const bool lower_case = false;
      return ArmenianAlgorithm(abs_value, lower_case);
    }
    case CounterStyleSystem::kUpperArmenian: {
      const bool upper_case = true;
      return ArmenianAlgorithm(abs_value, upper_case);
    }
    case CounterStyleSystem::kEthiopicNumeric:
      return EthiopicNumericAlgorithm(abs_value);
    case CounterStyleSystem::kUnresolvedExtends:
      NOTREACHED_IN_MIGRATION();
      return String();
  }
}

String CounterStyle::IndexesToString(
    const Vector<wtf_size_t>& symbol_indexes) const {
  if (symbol_indexes.empty()) {
    return String();
  }

  StringBuilder result;
  for (wtf_size_t index : symbol_indexes) {
    result.Append(symbols_[index]);
  }
  return result.ReleaseString();
}

void CounterStyle::TraverseAndMarkDirtyIfNeeded(
    HeapHashSet<Member<CounterStyle>>& visited_counter_styles) {
  if (IsPredefined() || visited_counter_styles.Contains(this)) {
    return;
  }
  visited_counter_styles.insert(this);

  if (has_inexistent_references_ ||
      style_rule_version_ != style_rule_->GetVersion()) {
    SetIsDirty();
    return;
  }

  if (extended_style_) {
    extended_style_->TraverseAndMarkDirtyIfNeeded(visited_counter_styles);
    if (extended_style_->IsDirty()) {
      SetIsDirty();
      return;
    }
  }

  if (fallback_style_) {
    fallback_style_->TraverseAndMarkDirtyIfNeeded(visited_counter_styles);
    if (fallback_style_->IsDirty()) {
      SetIsDirty();
      return;
    }
  }
}

CounterStyleSpeakAs CounterStyle::EffectiveSpeakAs() const {
  switch (speak_as_) {
    case CounterStyleSpeakAs::kBullets:
    case CounterStyleSpeakAs::kNumbers:
    case CounterStyleSpeakAs::kWords:
      return speak_as_;
    case CounterStyleSpeakAs::kReference:
      return GetSpeakAsStyle().EffectiveSpeakAs();
    case CounterStyleSpeakAs::kAuto:
      switch (system_) {
        case CounterStyleSystem::kCyclic:
          return CounterStyleSpeakAs::kBullets;
        case CounterStyleSystem::kAlphabetic:
          // Spec requires 'spell-out', which we don't support. Use 'words'
          // instead as the best effort, and also to align with Firefox.
          return CounterStyleSpeakAs::kWords;
        case CounterStyleSystem::kFixed:
        case CounterStyleSystem::kSymbolic:
        case CounterStyleSystem::kNumeric:
        case CounterStyleSystem::kAdditive:
        case CounterStyleSystem::kHebrew:
        case CounterStyleSystem::kLowerArmenian:
        case CounterStyleSystem::kUpperArmenian:
        case CounterStyleSystem::kSimpChineseInformal:
        case CounterStyleSystem::kSimpChineseFormal:
        case CounterStyleSystem::kTradChineseInformal:
        case CounterStyleSystem::kTradChineseFormal:
        case CounterStyleSystem::kKoreanHangulFormal:
        case CounterStyleSystem::kKoreanHanjaInformal:
        case CounterStyleSystem::kKoreanHanjaFormal:
        case CounterStyleSystem::kEthiopicNumeric:
          return CounterStyleSpeakAs::kNumbers;
        case CounterStyleSystem::kUnresolvedExtends:
          NOTREACHED_IN_MIGRATION();
          return CounterStyleSpeakAs::kNumbers;
      }
  }
}

String CounterStyle::GenerateTextAlternative(int value) const {
  if (!RuntimeEnabledFeatures::
          CSSAtRuleCounterStyleSpeakAsDescriptorEnabled()) {
    return GenerateRepresentationWithPrefixAndSuffix(value);
  }

  String text_without_prefix_suffix =
      GenerateTextAlternativeWithoutPrefixSuffix(value);

  // 'bullets' requires "a UA-defined phrase or audio cue", so we cannot use
  // custom prefix or suffix. Use the suffix of the predefined symbolic
  // styles instead.
  if (EffectiveSpeakAs() == CounterStyleSpeakAs::kBullets) {
    return text_without_prefix_suffix + " ";
  }

  return prefix_ + text_without_prefix_suffix + suffix_;
}

String CounterStyle::GenerateTextAlternativeWithoutPrefixSuffix(
    int value) const {
  if (speak_as_ == CounterStyleSpeakAs::kReference) {
    return GetSpeakAsStyle().GenerateTextAlternativeWithoutPrefixSuffix(value);
  }

  switch (EffectiveSpeakAs()) {
    case CounterStyleSpeakAs::kNumbers:
      return GetDecimal().GenerateRepresentation(value);
    case CounterStyleSpeakAs::kBullets:
      if (IsPredefinedSymbolMarker()) {
        return GenerateRepresentation(value);
      }
      return GetDisc().GenerateRepresentation(value);
    case CounterStyleSpeakAs::kWords:
      return GenerateRepresentation(value);
    case CounterStyleSpeakAs::kAuto:
    case CounterStyleSpeakAs::kReference:
      NOTREACHED_IN_MIGRATION();
      return String();
  }
}

void CounterStyle::Trace(Visitor* visitor) const {
  visitor->Trace(style_rule_);
  visitor->Trace(extended_style_);
  visitor->Trace(fallback_style_);
  visitor->Trace(speak_as_style_);
}

}  // namespace blink
