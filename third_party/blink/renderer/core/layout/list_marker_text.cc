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

#include "third_party/blink/renderer/core/layout/list_marker_text.h"

#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace list_marker_text {

enum SequenceType { kNumericSequence, kAlphabeticSequence };

static String ToRoman(int number, bool upper) {
  // FIXME: CSS3 describes how to make this work for much larger numbers,
  // using overbars and special characters. It also specifies the characters
  // in the range U+2160 to U+217F instead of standard ASCII ones.
  DCHECK_GE(number, 1);
  DCHECK_LE(number, 3999);

  // Big enough to store largest roman number less than 3999 which
  // is 3888 (MMMDCCCLXXXVIII)
  const int kLettersSize = 15;
  LChar letters[kLettersSize];

  int length = 0;
  const LChar kLdigits[] = {'i', 'v', 'x', 'l', 'c', 'd', 'm'};
  const LChar kUdigits[] = {'I', 'V', 'X', 'L', 'C', 'D', 'M'};
  const LChar* digits = upper ? kUdigits : kLdigits;
  int d = 0;
  do {
    int num = number % 10;
    if (num % 5 < 4)
      for (int i = num % 5; i > 0; i--)
        letters[kLettersSize - ++length] = digits[d];
    if (num >= 4 && num <= 8)
      letters[kLettersSize - ++length] = digits[d + 1];
    if (num == 9)
      letters[kLettersSize - ++length] = digits[d + 2];
    if (num % 5 == 4)
      letters[kLettersSize - ++length] = digits[d];
    number /= 10;
    d += 2;
  } while (number);

  DCHECK_LE(length, kLettersSize);
  return String(&letters[kLettersSize - length], length);
}

// The typedef is needed because taking sizeof(number) in the const expression
// below doesn't work with some compilers. This is likely the case because of
// the template.
typedef int numberType;

template <typename CharacterType>
static inline String ToAlphabeticOrNumeric(numberType number,
                                           const CharacterType* sequence,
                                           unsigned sequence_size,
                                           SequenceType type) {
  DCHECK_GE(sequence_size, 2u);

  // Binary is the worst case; requires one character per bit plus a minus sign.
  const int kLettersSize = sizeof(numberType) * 8 + 1;

  CharacterType letters[kLettersSize];

  bool is_negative_number = false;
  unsigned number_shadow = number;
  if (type == kAlphabeticSequence) {
    DCHECK_GT(number, 0);
    --number_shadow;
  } else if (number < 0) {
    number_shadow = -number;
    is_negative_number = true;
  }
  letters[kLettersSize - 1] = sequence[number_shadow % sequence_size];
  int length = 1;

  if (type == kAlphabeticSequence) {
    while ((number_shadow /= sequence_size) > 0) {
      --number_shadow;
      letters[kLettersSize - ++length] =
          sequence[number_shadow % sequence_size];
    }
  } else {
    while ((number_shadow /= sequence_size) > 0)
      letters[kLettersSize - ++length] =
          sequence[number_shadow % sequence_size];
  }
  if (is_negative_number)
    letters[kLettersSize - ++length] = kHyphenMinusCharacter;

  DCHECK_LE(length, kLettersSize);
  return String(&letters[kLettersSize - length], length);
}

template <typename CharacterType>
static String ToAlphabetic(int number,
                           const CharacterType* alphabet,
                           unsigned alphabet_size) {
  return ToAlphabeticOrNumeric(number, alphabet, alphabet_size,
                               kAlphabeticSequence);
}

template <typename CharacterType>
static String ToNumeric(int number,
                        const CharacterType* numerals,
                        unsigned numerals_size) {
  return ToAlphabeticOrNumeric(number, numerals, numerals_size,
                               kNumericSequence);
}

template <typename CharacterType, size_t size>
static inline String ToAlphabetic(int number,
                                  const CharacterType (&alphabet)[size]) {
  return ToAlphabetic(number, alphabet, size);
}

template <typename CharacterType, size_t size>
static inline String ToNumeric(int number,
                               const CharacterType (&alphabet)[size]) {
  return ToNumeric(number, alphabet, size);
}

static void ToHebrewUnder1000(int number, Vector<UChar>& letters) {
  // FIXME: CSS3 mentions various refinements not implemented here.
  // FIXME: Should take a look at Mozilla's HebrewToText function (in
  // nsBulletFrame).
  DCHECK_GE(number, 0);
  DCHECK_LT(number, 1000);
  int four_hundreds = number / 400;
  for (int i = 0; i < four_hundreds; i++)
    letters.push_front(1511 + 3);
  number %= 400;
  if (number / 100)
    letters.push_front(1511 + (number / 100) - 1);
  number %= 100;
  if (number == 15 || number == 16) {
    letters.push_front(1487 + 9);
    letters.push_front(1487 + number - 9);
  } else {
    if (int tens = number / 10) {
      static const UChar kHebrewTens[9] = {1497, 1499, 1500, 1502, 1504,
                                           1505, 1506, 1508, 1510};
      letters.push_front(kHebrewTens[tens - 1]);
    }
    if (int ones = number % 10)
      letters.push_front(1487 + ones);
  }
}

static String ToHebrew(int number) {
  // FIXME: CSS3 mentions ways to make this work for much larger numbers.
  DCHECK_GE(number, 0);
  DCHECK_LE(number, 999999);

  if (number == 0) {
    static const UChar kHebrewZero[3] = {0x05E1, 0x05E4, 0x05D0};
    return String(kHebrewZero, 3);
  }

  Vector<UChar> letters;
  if (number > 999) {
    ToHebrewUnder1000(number / 1000, letters);
    letters.push_front('\'');
    number = number % 1000;
  }
  ToHebrewUnder1000(number, letters);
  return String(letters);
}

static int ToArmenianUnder10000(int number,
                                bool upper,
                                bool add_circumflex,
                                UChar letters[9]) {
  DCHECK_GE(number, 0);
  DCHECK_LT(number, 10000);
  int length = 0;

  int lower_offset = upper ? 0 : 0x0030;

  if (int thousands = number / 1000) {
    if (thousands == 7) {
      letters[length++] = 0x0552 + lower_offset;
      if (add_circumflex)
        letters[length++] = 0x0302;
    } else {
      letters[length++] = (0x054C - 1 + lower_offset) + thousands;
      if (add_circumflex)
        letters[length++] = 0x0302;
    }
  }

  if (int hundreds = (number / 100) % 10) {
    letters[length++] = (0x0543 - 1 + lower_offset) + hundreds;
    if (add_circumflex)
      letters[length++] = 0x0302;
  }

  if (int tens = (number / 10) % 10) {
    letters[length++] = (0x053A - 1 + lower_offset) + tens;
    if (add_circumflex)
      letters[length++] = 0x0302;
  }

  if (int ones = number % 10) {
    letters[length++] = (0x531 - 1 + lower_offset) + ones;
    if (add_circumflex)
      letters[length++] = 0x0302;
  }

  return length;
}

static String ToArmenian(int number, bool upper) {
  DCHECK_GE(number, 1);
  DCHECK_LE(number, 99999999);

  const int kLettersSize = 18;  // twice what toArmenianUnder10000 needs
  UChar letters[kLettersSize];

  int length = ToArmenianUnder10000(number / 10000, upper, true, letters);
  length +=
      ToArmenianUnder10000(number % 10000, upper, false, letters + length);

  DCHECK_LE(length, kLettersSize);
  return String(letters, length);
}

static String ToGeorgian(int number) {
  DCHECK_GE(number, 1);
  DCHECK_LE(number, 19999);

  const int kLettersSize = 5;
  UChar letters[kLettersSize];

  int length = 0;

  if (number > 9999)
    letters[length++] = 0x10F5;

  if (int thousands = (number / 1000) % 10) {
    static const UChar kGeorgianThousands[9] = {
        0x10E9, 0x10EA, 0x10EB, 0x10EC, 0x10ED, 0x10EE, 0x10F4, 0x10EF, 0x10F0};
    letters[length++] = kGeorgianThousands[thousands - 1];
  }

  if (int hundreds = (number / 100) % 10) {
    static const UChar kGeorgianHundreds[9] = {
        0x10E0, 0x10E1, 0x10E2, 0x10F3, 0x10E4, 0x10E5, 0x10E6, 0x10E7, 0x10E8};
    letters[length++] = kGeorgianHundreds[hundreds - 1];
  }

  if (int tens = (number / 10) % 10) {
    static const UChar kGeorgianTens[9] = {
        0x10D8, 0x10D9, 0x10DA, 0x10DB, 0x10DC, 0x10F2, 0x10DD, 0x10DE, 0x10DF};
    letters[length++] = kGeorgianTens[tens - 1];
  }

  if (int ones = number % 10) {
    static const UChar kGeorgianOnes[9] = {
        0x10D0, 0x10D1, 0x10D2, 0x10D3, 0x10D4, 0x10D5, 0x10D6, 0x10F1, 0x10D7};
    letters[length++] = kGeorgianOnes[ones - 1];
  }

  DCHECK_LE(length, kLettersSize);
  return String(letters, length);
}

enum CJKLang { kChinese = 1, kKorean, kJapanese };

enum CJKStyle { kFormal, kInformal };

// The table uses the order from the CSS3 specification:
// first 3 group markers, then 3 digit markers, then ten digits, then negative
// symbols.
static String ToCJKIdeographic(int number,
                               const UChar table[26],
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
    kDigit9,
    kNeg1,
    kNeg2,
    kNeg3,
    kNeg4,
    kNeg5
  };

  if (number == 0)
    return String(&table[kDigit0], 1);

  const bool negative = number < 0;
  if (negative) {
    // Negating the most negative integer (INT_MIN) doesn't work, since it has
    // no positive counterpart. Deal with that here, manually.
    if (UNLIKELY(number == INT_MIN))
      number = INT_MAX;
    else
      number = -number;
  }

  const int kGroupLength =
      9;  // 4 digits, 3 digit markers, group marker of size 2.
  const int kBufferLength = 4 * kGroupLength;
  AbstractCJKChar buffer[kBufferLength] = {kNoChar};

  for (int i = 0; i < 4; ++i) {
    int group_value = number % 10000;
    number /= 10000;

    // Process least-significant group first, but put it in the buffer last.
    AbstractCJKChar* group = &buffer[(3 - i) * kGroupLength];

    if (group_value && i) {
      group[8] = static_cast<AbstractCJKChar>(kSecondGroupMarker + i);
      group[7] = static_cast<AbstractCJKChar>(kSecondGroupMarker - 1 + i);
    }

    // Put in the four digits and digit markers for any non-zero digits.
    int digit_value = (group_value % 10);
    bool trailing_zero = table[kLang] == kChinese && !digit_value;
    if (digit_value) {
      bool drop_one = table[kLang] == kKorean && cjk_style == kInformal &&
                      digit_value == 1 && i > 0;
      if (!drop_one)
        group[6] = static_cast<AbstractCJKChar>(kDigit0 + (group_value % 10));
    }
    if (number != 0 || group_value > 9) {
      digit_value = ((group_value / 10) % 10);
      bool drop_one =
          table[kLang] == kKorean && cjk_style == kInformal && digit_value == 1;
      if ((digit_value && !drop_one) || (!digit_value && !trailing_zero))
        group[4] = static_cast<AbstractCJKChar>(kDigit0 + digit_value);
      trailing_zero &= !digit_value;
      if (digit_value)
        group[5] = kSecondDigitMarker;
    }
    if (number != 0 || group_value > 99) {
      digit_value = ((group_value / 100) % 10);
      bool drop_one =
          table[kLang] == kKorean && cjk_style == kInformal && digit_value == 1;
      if ((digit_value && !drop_one) || (!digit_value && !trailing_zero))
        group[2] = static_cast<AbstractCJKChar>(kDigit0 + digit_value);
      trailing_zero &= !digit_value;
      if (digit_value)
        group[3] = kThirdDigitMarker;
    }
    if (number != 0 || group_value > 999) {
      digit_value = group_value / 1000;
      bool drop_one =
          table[kLang] == kKorean && cjk_style == kInformal && digit_value == 1;
      if ((digit_value && !drop_one) || (!digit_value && !trailing_zero))
        group[0] = static_cast<AbstractCJKChar>(kDigit0 + digit_value);
      if (digit_value)
        group[1] = kFourthDigitMarker;
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

    if (number == 0)
      break;
  }

  // Convert into characters, omitting consecutive runs of Digit0 and
  // any trailing Digit0.
  int length = 0;
  const int kMaxLengthForNegativeSymbols = 5;
  UChar characters[kBufferLength + kMaxLengthForNegativeSymbols];
  AbstractCJKChar last = kNoChar;
  if (negative) {
    while (UChar a = table[kNeg1 + length])
      characters[length++] = a;
  }
  for (int i = 0; i < kBufferLength; ++i) {
    AbstractCJKChar a = buffer[i];
    if (a != kNoChar) {
      if (a != kDigit0 || (table[kLang] == kChinese && last != kDigit0)) {
        UChar new_char = table[a];
        if (new_char != kNoChar) {
          characters[length++] = table[a];
          if (table[kLang] == kKorean &&
              (a == kSecondGroupMarker || a == kThirdGroupMarker ||
               a == kFourthGroupMarker))
            characters[length++] = ' ';
        }
      }
      last = a;
    }
  }
  if ((table[kLang] == kChinese && last == kDigit0) ||
      characters[length - 1] == ' ')
    --length;

  return String(characters, length);
}

static EListStyleType EffectiveListMarkerType(EListStyleType type, int count) {
  // Note, the following switch statement has been explicitly grouped
  // by list-style-type ordinal range.
  switch (type) {
    case EListStyleType::kArabicIndic:
    case EListStyleType::kBengali:
    case EListStyleType::kCambodian:
    case EListStyleType::kCircle:
    case EListStyleType::kDecimalLeadingZero:
    case EListStyleType::kDecimal:
    case EListStyleType::kDevanagari:
    case EListStyleType::kDisc:
    case EListStyleType::kGujarati:
    case EListStyleType::kGurmukhi:
    case EListStyleType::kKannada:
    case EListStyleType::kKhmer:
    case EListStyleType::kLao:
    case EListStyleType::kMalayalam:
    case EListStyleType::kMongolian:
    case EListStyleType::kMyanmar:
    case EListStyleType::kNone:
    case EListStyleType::kOriya:
    case EListStyleType::kPersian:
    case EListStyleType::kSquare:
    case EListStyleType::kTelugu:
    case EListStyleType::kThai:
    case EListStyleType::kTibetan:
    case EListStyleType::kUrdu:
    case EListStyleType::kKoreanHangulFormal:
    case EListStyleType::kKoreanHanjaFormal:
    case EListStyleType::kKoreanHanjaInformal:
    case EListStyleType::kCjkIdeographic:
    case EListStyleType::kSimpChineseFormal:
    case EListStyleType::kSimpChineseInformal:
    case EListStyleType::kTradChineseFormal:
    case EListStyleType::kTradChineseInformal:
      return type;  // Can represent all ordinals.
    case EListStyleType::kArmenian:
    case EListStyleType::kLowerArmenian:
    case EListStyleType::kUpperArmenian:
      return (count < 1 || count > 99999999) ? EListStyleType::kDecimal : type;
    case EListStyleType::kGeorgian:
      return (count < 1 || count > 19999) ? EListStyleType::kDecimal : type;
    case EListStyleType::kHebrew:
      return (count < 0 || count > 999999) ? EListStyleType::kDecimal : type;
    case EListStyleType::kLowerRoman:
    case EListStyleType::kUpperRoman:
      return (count < 1 || count > 3999) ? EListStyleType::kDecimal : type;
    case EListStyleType::kCjkEarthlyBranch:
    case EListStyleType::kCjkHeavenlyStem:
    case EListStyleType::kEthiopicHalehameAm:
    case EListStyleType::kEthiopicHalehame:
    case EListStyleType::kEthiopicHalehameTiEr:
    case EListStyleType::kEthiopicHalehameTiEt:
    case EListStyleType::kHangul:
    case EListStyleType::kHangulConsonant:
    case EListStyleType::kHiragana:
    case EListStyleType::kHiraganaIroha:
    case EListStyleType::kKatakana:
    case EListStyleType::kKatakanaIroha:
    case EListStyleType::kLowerAlpha:
    case EListStyleType::kLowerGreek:
    case EListStyleType::kLowerLatin:
    case EListStyleType::kUpperAlpha:
    case EListStyleType::kUpperLatin:
      return (count < 1) ? EListStyleType::kDecimal : type;
    case EListStyleType::kString:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return type;
}

UChar Suffix(EListStyleType type, int count) {
  // If the list-style-type cannot represent |count| because it's outside its
  // ordinal range then we fall back to some list style that can represent
  // |count|.
  EListStyleType effective_type = EffectiveListMarkerType(type, count);

  // Note, the following switch statement has been explicitly
  // grouped by list-style-type suffix.
  switch (effective_type) {
    case EListStyleType::kCircle:
    case EListStyleType::kDisc:
    case EListStyleType::kNone:
    case EListStyleType::kSquare:
      return ' ';
    case EListStyleType::kEthiopicHalehame:
    case EListStyleType::kEthiopicHalehameAm:
    case EListStyleType::kEthiopicHalehameTiEr:
    case EListStyleType::kEthiopicHalehameTiEt:
      return kEthiopicPrefaceColonCharacter;
    case EListStyleType::kArmenian:
    case EListStyleType::kArabicIndic:
    case EListStyleType::kBengali:
    case EListStyleType::kCambodian:
    case EListStyleType::kCjkIdeographic:
    case EListStyleType::kCjkEarthlyBranch:
    case EListStyleType::kCjkHeavenlyStem:
    case EListStyleType::kDecimalLeadingZero:
    case EListStyleType::kDecimal:
    case EListStyleType::kDevanagari:
    case EListStyleType::kGeorgian:
    case EListStyleType::kGujarati:
    case EListStyleType::kGurmukhi:
    case EListStyleType::kHangul:
    case EListStyleType::kHangulConsonant:
    case EListStyleType::kHebrew:
    case EListStyleType::kHiragana:
    case EListStyleType::kHiraganaIroha:
    case EListStyleType::kKannada:
    case EListStyleType::kKatakana:
    case EListStyleType::kKatakanaIroha:
    case EListStyleType::kKhmer:
    case EListStyleType::kLao:
    case EListStyleType::kLowerAlpha:
    case EListStyleType::kLowerArmenian:
    case EListStyleType::kLowerGreek:
    case EListStyleType::kLowerLatin:
    case EListStyleType::kLowerRoman:
    case EListStyleType::kMalayalam:
    case EListStyleType::kMongolian:
    case EListStyleType::kMyanmar:
    case EListStyleType::kOriya:
    case EListStyleType::kPersian:
    case EListStyleType::kTelugu:
    case EListStyleType::kThai:
    case EListStyleType::kTibetan:
    case EListStyleType::kUpperAlpha:
    case EListStyleType::kUpperArmenian:
    case EListStyleType::kUpperLatin:
    case EListStyleType::kUpperRoman:
    case EListStyleType::kUrdu:
      return '.';
    case EListStyleType::kSimpChineseFormal:
    case EListStyleType::kSimpChineseInformal:
    case EListStyleType::kTradChineseFormal:
    case EListStyleType::kTradChineseInformal:
    case EListStyleType::kKoreanHangulFormal:
    case EListStyleType::kKoreanHanjaFormal:
    case EListStyleType::kKoreanHanjaInformal:
      return 0x3001;
    case EListStyleType::kString:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return '.';
}

String GetText(EListStyleType type, int count) {
  // If the list-style-type, say hebrew, cannot represent |count| because it's
  // outside its ordinal range then we fallback to some list style that can
  // represent |count|.
  switch (EffectiveListMarkerType(type, count)) {
    case EListStyleType::kNone:
      return "";

    // We use the same characters for text security.
    // See LayoutText::setInternalString.
    case EListStyleType::kCircle:
      return String(&kWhiteBulletCharacter, 1);
    case EListStyleType::kDisc:
      return String(&kBulletCharacter, 1);
    case EListStyleType::kSquare:
      // The CSS 2.1 test suite uses U+25EE BLACK MEDIUM SMALL SQUARE
      // instead, but I think this looks better.
      return String(&kBlackSquareCharacter, 1);

    case EListStyleType::kDecimal:
      return String::Number(count);
    case EListStyleType::kDecimalLeadingZero:
      if (count < -9 || count > 9)
        return String::Number(count);
      if (count < 0)
        return "-0" + String::Number(-count);  // -01 to -09
      return "0" + String::Number(count);      // 00 to 09

    case EListStyleType::kArabicIndic: {
      static const UChar kArabicIndicNumerals[10] = {
          0x0660, 0x0661, 0x0662, 0x0663, 0x0664,
          0x0665, 0x0666, 0x0667, 0x0668, 0x0669};
      return ToNumeric(count, kArabicIndicNumerals);
    }
    case EListStyleType::kBengali: {
      static const UChar kBengaliNumerals[10] = {0x09E6, 0x09E7, 0x09E8, 0x09E9,
                                                 0x09EA, 0x09EB, 0x09EC, 0x09ED,
                                                 0x09EE, 0x09EF};
      return ToNumeric(count, kBengaliNumerals);
    }
    case EListStyleType::kCambodian:
    case EListStyleType::kKhmer: {
      static const UChar kKhmerNumerals[10] = {0x17E0, 0x17E1, 0x17E2, 0x17E3,
                                               0x17E4, 0x17E5, 0x17E6, 0x17E7,
                                               0x17E8, 0x17E9};
      return ToNumeric(count, kKhmerNumerals);
    }
    case EListStyleType::kDevanagari: {
      static const UChar kDevanagariNumerals[10] = {
          0x0966, 0x0967, 0x0968, 0x0969, 0x096A,
          0x096B, 0x096C, 0x096D, 0x096E, 0x096F};
      return ToNumeric(count, kDevanagariNumerals);
    }
    case EListStyleType::kGujarati: {
      static const UChar kGujaratiNumerals[10] = {
          0x0AE6, 0x0AE7, 0x0AE8, 0x0AE9, 0x0AEA,
          0x0AEB, 0x0AEC, 0x0AED, 0x0AEE, 0x0AEF};
      return ToNumeric(count, kGujaratiNumerals);
    }
    case EListStyleType::kGurmukhi: {
      static const UChar kGurmukhiNumerals[10] = {
          0x0A66, 0x0A67, 0x0A68, 0x0A69, 0x0A6A,
          0x0A6B, 0x0A6C, 0x0A6D, 0x0A6E, 0x0A6F};
      return ToNumeric(count, kGurmukhiNumerals);
    }
    case EListStyleType::kKannada: {
      static const UChar kKannadaNumerals[10] = {0x0CE6, 0x0CE7, 0x0CE8, 0x0CE9,
                                                 0x0CEA, 0x0CEB, 0x0CEC, 0x0CED,
                                                 0x0CEE, 0x0CEF};
      return ToNumeric(count, kKannadaNumerals);
    }
    case EListStyleType::kLao: {
      static const UChar kLaoNumerals[10] = {0x0ED0, 0x0ED1, 0x0ED2, 0x0ED3,
                                             0x0ED4, 0x0ED5, 0x0ED6, 0x0ED7,
                                             0x0ED8, 0x0ED9};
      return ToNumeric(count, kLaoNumerals);
    }
    case EListStyleType::kMalayalam: {
      static const UChar kMalayalamNumerals[10] = {
          0x0D66, 0x0D67, 0x0D68, 0x0D69, 0x0D6A,
          0x0D6B, 0x0D6C, 0x0D6D, 0x0D6E, 0x0D6F};
      return ToNumeric(count, kMalayalamNumerals);
    }
    case EListStyleType::kMongolian: {
      static const UChar kMongolianNumerals[10] = {
          0x1810, 0x1811, 0x1812, 0x1813, 0x1814,
          0x1815, 0x1816, 0x1817, 0x1818, 0x1819};
      return ToNumeric(count, kMongolianNumerals);
    }
    case EListStyleType::kMyanmar: {
      static const UChar kMyanmarNumerals[10] = {0x1040, 0x1041, 0x1042, 0x1043,
                                                 0x1044, 0x1045, 0x1046, 0x1047,
                                                 0x1048, 0x1049};
      return ToNumeric(count, kMyanmarNumerals);
    }
    case EListStyleType::kOriya: {
      static const UChar kOriyaNumerals[10] = {0x0B66, 0x0B67, 0x0B68, 0x0B69,
                                               0x0B6A, 0x0B6B, 0x0B6C, 0x0B6D,
                                               0x0B6E, 0x0B6F};
      return ToNumeric(count, kOriyaNumerals);
    }
    case EListStyleType::kPersian:
    case EListStyleType::kUrdu: {
      static const UChar kUrduNumerals[10] = {0x06F0, 0x06F1, 0x06F2, 0x06F3,
                                              0x06F4, 0x06F5, 0x06F6, 0x06F7,
                                              0x06F8, 0x06F9};
      return ToNumeric(count, kUrduNumerals);
    }
    case EListStyleType::kTelugu: {
      static const UChar kTeluguNumerals[10] = {0x0C66, 0x0C67, 0x0C68, 0x0C69,
                                                0x0C6A, 0x0C6B, 0x0C6C, 0x0C6D,
                                                0x0C6E, 0x0C6F};
      return ToNumeric(count, kTeluguNumerals);
    }
    case EListStyleType::kTibetan: {
      static const UChar kTibetanNumerals[10] = {0x0F20, 0x0F21, 0x0F22, 0x0F23,
                                                 0x0F24, 0x0F25, 0x0F26, 0x0F27,
                                                 0x0F28, 0x0F29};
      return ToNumeric(count, kTibetanNumerals);
    }
    case EListStyleType::kThai: {
      static const UChar kThaiNumerals[10] = {0x0E50, 0x0E51, 0x0E52, 0x0E53,
                                              0x0E54, 0x0E55, 0x0E56, 0x0E57,
                                              0x0E58, 0x0E59};
      return ToNumeric(count, kThaiNumerals);
    }

    case EListStyleType::kLowerAlpha:
    case EListStyleType::kLowerLatin: {
      static const LChar kLowerLatinAlphabet[26] = {
          'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
          'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
      return ToAlphabetic(count, kLowerLatinAlphabet);
    }
    case EListStyleType::kUpperAlpha:
    case EListStyleType::kUpperLatin: {
      static const LChar kUpperLatinAlphabet[26] = {
          'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
          'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
      return ToAlphabetic(count, kUpperLatinAlphabet);
    }
    case EListStyleType::kLowerGreek: {
      static const UChar kLowerGreekAlphabet[24] = {
          0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8,
          0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF, 0x03C0,
          0x03C1, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9};
      return ToAlphabetic(count, kLowerGreekAlphabet);
    }

    case EListStyleType::kHiragana: {
      // FIXME: This table comes from the CSS3 draft, and is probably
      // incorrect, given the comments in that draft.
      static const UChar kHiraganaAlphabet[48] = {
          0x3042, 0x3044, 0x3046, 0x3048, 0x304A, 0x304B, 0x304D, 0x304F,
          0x3051, 0x3053, 0x3055, 0x3057, 0x3059, 0x305B, 0x305D, 0x305F,
          0x3061, 0x3064, 0x3066, 0x3068, 0x306A, 0x306B, 0x306C, 0x306D,
          0x306E, 0x306F, 0x3072, 0x3075, 0x3078, 0x307B, 0x307E, 0x307F,
          0x3080, 0x3081, 0x3082, 0x3084, 0x3086, 0x3088, 0x3089, 0x308A,
          0x308B, 0x308C, 0x308D, 0x308F, 0x3090, 0x3091, 0x3092, 0x3093};
      return ToAlphabetic(count, kHiraganaAlphabet);
    }
    case EListStyleType::kHiraganaIroha: {
      // FIXME: This table comes from the CSS3 draft, and is probably
      // incorrect, given the comments in that draft.
      static const UChar kHiraganaIrohaAlphabet[47] = {
          0x3044, 0x308D, 0x306F, 0x306B, 0x307B, 0x3078, 0x3068, 0x3061,
          0x308A, 0x306C, 0x308B, 0x3092, 0x308F, 0x304B, 0x3088, 0x305F,
          0x308C, 0x305D, 0x3064, 0x306D, 0x306A, 0x3089, 0x3080, 0x3046,
          0x3090, 0x306E, 0x304A, 0x304F, 0x3084, 0x307E, 0x3051, 0x3075,
          0x3053, 0x3048, 0x3066, 0x3042, 0x3055, 0x304D, 0x3086, 0x3081,
          0x307F, 0x3057, 0x3091, 0x3072, 0x3082, 0x305B, 0x3059};
      return ToAlphabetic(count, kHiraganaIrohaAlphabet);
    }
    case EListStyleType::kKatakana: {
      // FIXME: This table comes from the CSS3 draft, and is probably
      // incorrect, given the comments in that draft.
      static const UChar kKatakanaAlphabet[48] = {
          0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF,
          0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF,
          0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD,
          0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF,
          0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA,
          0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F0, 0x30F1, 0x30F2, 0x30F3};
      return ToAlphabetic(count, kKatakanaAlphabet);
    }
    case EListStyleType::kKatakanaIroha: {
      // FIXME: This table comes from the CSS3 draft, and is probably
      // incorrect, given the comments in that draft.
      static const UChar kKatakanaIrohaAlphabet[47] = {
          0x30A4, 0x30ED, 0x30CF, 0x30CB, 0x30DB, 0x30D8, 0x30C8, 0x30C1,
          0x30EA, 0x30CC, 0x30EB, 0x30F2, 0x30EF, 0x30AB, 0x30E8, 0x30BF,
          0x30EC, 0x30BD, 0x30C4, 0x30CD, 0x30CA, 0x30E9, 0x30E0, 0x30A6,
          0x30F0, 0x30CE, 0x30AA, 0x30AF, 0x30E4, 0x30DE, 0x30B1, 0x30D5,
          0x30B3, 0x30A8, 0x30C6, 0x30A2, 0x30B5, 0x30AD, 0x30E6, 0x30E1,
          0x30DF, 0x30B7, 0x30F1, 0x30D2, 0x30E2, 0x30BB, 0x30B9};
      return ToAlphabetic(count, kKatakanaIrohaAlphabet);
    }

    case EListStyleType::kCjkEarthlyBranch: {
      static const UChar kCjkEarthlyBranchAlphabet[12] = {
          0x5B50, 0x4E11, 0x5BC5, 0x536F, 0x8FB0, 0x5DF3,
          0x5348, 0x672A, 0x7533, 0x9149, 0x620C, 0x4EA5};
      return ToAlphabetic(count, kCjkEarthlyBranchAlphabet);
    }
    case EListStyleType::kCjkHeavenlyStem: {
      static const UChar kCjkHeavenlyStemAlphabet[10] = {
          0x7532, 0x4E59, 0x4E19, 0x4E01, 0x620A,
          0x5DF1, 0x5E9A, 0x8F9B, 0x58EC, 0x7678};
      return ToAlphabetic(count, kCjkHeavenlyStemAlphabet);
    }
    case EListStyleType::kHangulConsonant: {
      static const UChar kHangulConsonantAlphabet[14] = {
          0x3131, 0x3134, 0x3137, 0x3139, 0x3141, 0x3142, 0x3145,
          0x3147, 0x3148, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E};
      return ToAlphabetic(count, kHangulConsonantAlphabet);
    }
    case EListStyleType::kHangul: {
      static const UChar kHangulAlphabet[14] = {
          0xAC00, 0xB098, 0xB2E4, 0xB77C, 0xB9C8, 0xBC14, 0xC0AC,
          0xC544, 0xC790, 0xCC28, 0xCE74, 0xD0C0, 0xD30C, 0xD558};
      return ToAlphabetic(count, kHangulAlphabet);
    }
    case EListStyleType::kEthiopicHalehame: {
      static const UChar kEthiopicHalehameGezAlphabet[26] = {
          0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230,
          0x1240, 0x1260, 0x1270, 0x1280, 0x1290, 0x12A0, 0x12A8,
          0x12C8, 0x12D0, 0x12D8, 0x12E8, 0x12F0, 0x1308, 0x1320,
          0x1330, 0x1338, 0x1340, 0x1348, 0x1350};
      return ToAlphabetic(count, kEthiopicHalehameGezAlphabet);
    }
    case EListStyleType::kEthiopicHalehameAm: {
      static const UChar kEthiopicHalehameAmAlphabet[33] = {
          0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230,
          0x1238, 0x1240, 0x1260, 0x1270, 0x1278, 0x1280, 0x1290,
          0x1298, 0x12A0, 0x12A8, 0x12B8, 0x12C8, 0x12D0, 0x12D8,
          0x12E0, 0x12E8, 0x12F0, 0x1300, 0x1308, 0x1320, 0x1328,
          0x1330, 0x1338, 0x1340, 0x1348, 0x1350};
      return ToAlphabetic(count, kEthiopicHalehameAmAlphabet);
    }
    case EListStyleType::kEthiopicHalehameTiEr: {
      static const UChar kEthiopicHalehameTiErAlphabet[31] = {
          0x1200, 0x1208, 0x1210, 0x1218, 0x1228, 0x1230, 0x1238, 0x1240,
          0x1250, 0x1260, 0x1270, 0x1278, 0x1290, 0x1298, 0x12A0, 0x12A8,
          0x12B8, 0x12C8, 0x12D0, 0x12D8, 0x12E0, 0x12E8, 0x12F0, 0x1300,
          0x1308, 0x1320, 0x1328, 0x1330, 0x1338, 0x1348, 0x1350};
      return ToAlphabetic(count, kEthiopicHalehameTiErAlphabet);
    }
    case EListStyleType::kEthiopicHalehameTiEt: {
      static const UChar kEthiopicHalehameTiEtAlphabet[34] = {
          0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230,
          0x1238, 0x1240, 0x1250, 0x1260, 0x1270, 0x1278, 0x1280,
          0x1290, 0x1298, 0x12A0, 0x12A8, 0x12B8, 0x12C8, 0x12D0,
          0x12D8, 0x12E0, 0x12E8, 0x12F0, 0x1300, 0x1308, 0x1320,
          0x1328, 0x1330, 0x1338, 0x1340, 0x1348, 0x1350};
      return ToAlphabetic(count, kEthiopicHalehameTiEtAlphabet);
    }
    case EListStyleType::kKoreanHangulFormal: {
      static const UChar kKoreanHangulFormalTable[26] = {
          kKorean, 0xB9CC, 0x0000, 0xC5B5, 0x0000, 0xC870, 0x0000,
          0xC2ED,  0xBC31, 0xCC9C, 0xC601, 0xC77C, 0xC774, 0xC0BC,
          0xC0AC,  0xC624, 0xC721, 0xCE60, 0xD314, 0xAD6C, 0xB9C8,
          0xC774,  0xB108, 0xC2A4, 0x0020, 0x0000};
      return ToCJKIdeographic(count, kKoreanHangulFormalTable, kFormal);
    }
    case EListStyleType::kKoreanHanjaFormal: {
      static const UChar kKoreanHanjaFormalTable[26] = {
          kKorean, 0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000,
          0x62FE,  0x767E, 0x4EDF, 0x96F6, 0x58F9, 0x8CB3, 0x53C3,
          0x56DB,  0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D, 0xB9C8,
          0xC774,  0xB108, 0xC2A4, 0x0020, 0x0000};
      return ToCJKIdeographic(count, kKoreanHanjaFormalTable, kFormal);
    }
    case EListStyleType::kKoreanHanjaInformal: {
      static const UChar kKoreanHanjaInformalTable[26] = {
          kKorean, 0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000,
          0x5341,  0x767E, 0x5343, 0x96F6, 0x4E00, 0x4E8C, 0x4E09,
          0x56DB,  0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D, 0xB9C8,
          0xC774,  0xB108, 0xC2A4, 0x0020, 0x0000};
      return ToCJKIdeographic(count, kKoreanHanjaInformalTable, kInformal);
    }
    case EListStyleType::kCjkIdeographic:
    case EListStyleType::kTradChineseInformal: {
      static const UChar kTraditionalChineseInformalTable[22] = {
          kChinese, 0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000, 0x5341,
          0x767E,   0x5343, 0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94,
          0x516D,   0x4E03, 0x516B, 0x4E5D, 0x8CA0, 0x0000};
      return ToCJKIdeographic(count, kTraditionalChineseInformalTable,
                              kInformal);
    }
    case EListStyleType::kSimpChineseInformal: {
      static const UChar kSimpleChineseInformalTable[22] = {
          kChinese, 0x4E07, 0x0000, 0x4EBF, 0x0000, 0x4E07, 0x4EBF, 0x5341,
          0x767E,   0x5343, 0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94,
          0x516D,   0x4E03, 0x516B, 0x4E5D, 0x8D1F, 0x0000};
      return ToCJKIdeographic(count, kSimpleChineseInformalTable, kInformal);
    }
    case EListStyleType::kTradChineseFormal: {
      static const UChar kTraditionalChineseFormalTable[22] = {
          kChinese, 0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000, 0x62FE,
          0x4F70,   0x4EDF, 0x96F6, 0x58F9, 0x8CB3, 0x53C3, 0x8086, 0x4F0D,
          0x9678,   0x67D2, 0x634C, 0x7396, 0x8CA0, 0x0000};
      return ToCJKIdeographic(count, kTraditionalChineseFormalTable, kFormal);
    }
    case EListStyleType::kSimpChineseFormal: {
      static const UChar kSimpleChineseFormalTable[22] = {
          kChinese, 0x4E07, 0x0000, 0x4EBF, 0x0000, 0x4E07, 0x4EBF, 0x62FE,
          0x4F70,   0x4EDF, 0x96F6, 0x58F9, 0x8D30, 0x53C1, 0x8086, 0x4F0D,
          0x9646,   0x67D2, 0x634C, 0x7396, 0x8D1F, 0x0000};
      return ToCJKIdeographic(count, kSimpleChineseFormalTable, kFormal);
    }

    case EListStyleType::kLowerRoman:
      return ToRoman(count, false);
    case EListStyleType::kUpperRoman:
      return ToRoman(count, true);

    case EListStyleType::kArmenian:
    case EListStyleType::kUpperArmenian:
      // CSS3 says "armenian" means "lower-armenian".
      // But the CSS2.1 test suite contains uppercase test results for
      // "armenian", so we'll match the test suite.
      return ToArmenian(count, true);
    case EListStyleType::kLowerArmenian:
      return ToArmenian(count, false);
    case EListStyleType::kGeorgian:
      return ToGeorgian(count);
    case EListStyleType::kHebrew:
      return ToHebrew(count);

    case EListStyleType::kString:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return "";
}

}  // namespace list_marker_text

}  // namespace blink
