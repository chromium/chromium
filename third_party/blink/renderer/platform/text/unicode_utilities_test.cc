/*
 * Copyright (c) 2013 Yandex LLC. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Yandex LLC nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/text/unicode_utilities.h"

#include <unicode/uchar.h>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

static const UChar32 kMaxLatinCharCount = 256;

static bool g_is_test_first_and_last_chars_in_category_failed = false;
UBool U_CALLCONV TestFirstAndLastCharsInCategory(const void* context,
                                                 UChar32 start,
                                                 UChar32 limit,
                                                 UCharCategory type) {
  if (start >= kMaxLatinCharCount &&
      U_MASK(type) & (U_GC_S_MASK | U_GC_P_MASK | U_GC_Z_MASK | U_GC_CF_MASK) &&
      (!IsSeparator(start) || !IsSeparator(limit - 1))) {
    g_is_test_first_and_last_chars_in_category_failed = true;

    // Break enumeration process
    return 0;
  }

  return 1;
}

TEST(UnicodeUtilitiesTest, Separators) {
  // clang-format off
  static const bool kLatinSeparatorTable[256] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      // space ! " # $ % & ' ( ) * + , - . /
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      //                         : ; < = > ?
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
      //   @
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      //                         [ \ ] ^ _
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
      //   `
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      //                           { | } ~
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0
  };
  // clang-format on

  for (UChar32 character = 0; character < kMaxLatinCharCount; ++character) {
    EXPECT_EQ(IsSeparator(character), kLatinSeparatorTable[character]);
  }

  g_is_test_first_and_last_chars_in_category_failed = false;
  u_enumCharTypes(&TestFirstAndLastCharsInCategory, nullptr);
  EXPECT_FALSE(g_is_test_first_and_last_chars_in_category_failed);
}

TEST(UnicodeUtilitiesTest, KanaLetters) {
  // Non Kana symbols
  for (UChar character = 0; character < 0x3041; ++character)
    EXPECT_FALSE(IsKanaLetter(character));

  // Hiragana letters.
  for (UChar character = 0x3041; character <= 0x3096; ++character)
    EXPECT_TRUE(IsKanaLetter(character));

  // Katakana letters.
  for (UChar character = 0x30A1; character <= 0x30FA; ++character)
    EXPECT_TRUE(IsKanaLetter(character));
}

TEST(UnicodeUtilitiesTest, ContainsKanaLetters) {
  // Non Kana symbols
  StringBuilder non_kana_string;
  for (UChar character = 0; character < 0x3041; ++character)
    non_kana_string.Append(character);
  EXPECT_FALSE(ContainsKanaLetters(non_kana_string.ToString()));

  // Hiragana letters.
  for (UChar character = 0x3041; character <= 0x3096; ++character) {
    StringBuilder str;
    str.Append(non_kana_string);
    str.Append(character);
    EXPECT_TRUE(ContainsKanaLetters(str.ToString()));
  }

  // Katakana letters.
  for (UChar character = 0x30A1; character <= 0x30FA; ++character) {
    StringBuilder str;
    str.Append(non_kana_string);
    str.Append(character);
    EXPECT_TRUE(ContainsKanaLetters(str.ToString()));
  }
}

TEST(UnicodeUtilitiesTest, FoldQuoteMarkOrSoftHyphenTest) {
  const UChar kCharactersToFold[] = {kHebrewPunctuationGershayimCharacter,
                                     kLeftDoubleQuotationMarkCharacter,
                                     kRightDoubleQuotationMarkCharacter,
                                     kHebrewPunctuationGereshCharacter,
                                     kLeftSingleQuotationMarkCharacter,
                                     kRightSingleQuotationMarkCharacter,
                                     kSoftHyphenCharacter};

  String string_to_fold(kCharactersToFold, base::size(kCharactersToFold));
  Vector<UChar> buffer;
  string_to_fold.AppendTo(buffer);

  FoldQuoteMarksAndSoftHyphens(string_to_fold);

  const String folded_string("\"\"\"\'\'\'\0", base::size(kCharactersToFold));
  EXPECT_EQ(string_to_fold, folded_string);

  FoldQuoteMarksAndSoftHyphens(buffer.data(), buffer.size());
  EXPECT_EQ(String(buffer), folded_string);
}

TEST(UnicodeUtilitiesTest, OnlyKanaLettersEqualityTest) {
  const UChar kNonKanaString1[] = {'a', 'b', 'c', 'd'};
  const UChar kNonKanaString2[] = {'e', 'f', 'g'};

  // Check that non-Kana letters will be skipped.
  EXPECT_TRUE(CheckOnlyKanaLettersInStrings(
      kNonKanaString1, base::size(kNonKanaString1), kNonKanaString2,
      base::size(kNonKanaString2)));

  const UChar kKanaString[] = {'e', 'f', 'g', 0x3041};
  EXPECT_FALSE(CheckOnlyKanaLettersInStrings(
      kKanaString, base::size(kKanaString), kNonKanaString2,
      base::size(kNonKanaString2)));

  // Compare with self.
  EXPECT_TRUE(
      CheckOnlyKanaLettersInStrings(kKanaString, base::size(kKanaString),
                                    kKanaString, base::size(kKanaString)));

  UChar voiced_kana_string1[] = {0x3042, 0x3099};
  UChar voiced_kana_string2[] = {0x3042, 0x309A};

  // Comparing strings with different sound marks should fail.
  EXPECT_FALSE(CheckOnlyKanaLettersInStrings(
      voiced_kana_string1, base::size(voiced_kana_string1), voiced_kana_string2,
      base::size(voiced_kana_string2)));

  // Now strings will be the same.
  voiced_kana_string2[1] = 0x3099;
  EXPECT_TRUE(CheckOnlyKanaLettersInStrings(
      voiced_kana_string1, base::size(voiced_kana_string1), voiced_kana_string2,
      base::size(voiced_kana_string2)));

  voiced_kana_string2[0] = 0x3043;
  EXPECT_FALSE(CheckOnlyKanaLettersInStrings(
      voiced_kana_string1, base::size(voiced_kana_string1), voiced_kana_string2,
      base::size(voiced_kana_string2)));
}

TEST(UnicodeUtilitiesTest, StringsWithKanaLettersTest) {
  const UChar kNonKanaString1[] = {'a', 'b', 'c'};
  const UChar kNonKanaString2[] = {'a', 'b', 'c'};

  // Check that non-Kana letters will be compared.
  EXPECT_TRUE(
      CheckKanaStringsEqual(kNonKanaString1, base::size(kNonKanaString1),
                            kNonKanaString2, base::size(kNonKanaString2)));

  const UChar kKanaString[] = {'a', 'b', 'c', 0x3041};
  EXPECT_FALSE(CheckKanaStringsEqual(kKanaString, base::size(kKanaString),
                                     kNonKanaString2,
                                     base::size(kNonKanaString2)));

  // Compare with self.
  EXPECT_TRUE(CheckKanaStringsEqual(kKanaString, base::size(kKanaString),
                                    kKanaString, base::size(kKanaString)));

  const UChar kKanaString2[] = {'x', 'y', 'z', 0x3041};
  // Comparing strings with different non-Kana letters should fail.
  EXPECT_FALSE(CheckKanaStringsEqual(kKanaString, base::size(kKanaString),
                                     kKanaString2, base::size(kKanaString2)));

  const UChar kKanaString3[] = {'a', 'b', 'c', 0x3042, 0x3099, 'm', 'n', 'o'};
  // Check that non-Kana letters after Kana letters will be compared.
  EXPECT_TRUE(CheckKanaStringsEqual(kKanaString3, base::size(kKanaString3),
                                    kKanaString3, base::size(kKanaString3)));

  const UChar kKanaString4[] = {'a', 'b', 'c', 0x3042, 0x3099,
                                'm', 'n', 'o', 'p'};
  // And now comparing should fail.
  EXPECT_FALSE(CheckKanaStringsEqual(kKanaString3, base::size(kKanaString3),
                                     kKanaString4, base::size(kKanaString4)));

  UChar voiced_kana_string1[] = {0x3042, 0x3099};
  UChar voiced_kana_string2[] = {0x3042, 0x309A};

  // Comparing strings with different sound marks should fail.
  EXPECT_FALSE(CheckKanaStringsEqual(
      voiced_kana_string1, base::size(voiced_kana_string1), voiced_kana_string2,
      base::size(voiced_kana_string2)));

  // Now strings will be the same.
  voiced_kana_string2[1] = 0x3099;
  EXPECT_TRUE(CheckKanaStringsEqual(
      voiced_kana_string1, base::size(voiced_kana_string1), voiced_kana_string2,
      base::size(voiced_kana_string2)));

  voiced_kana_string2[0] = 0x3043;
  EXPECT_FALSE(CheckKanaStringsEqual(
      voiced_kana_string1, base::size(voiced_kana_string1), voiced_kana_string2,
      base::size(voiced_kana_string2)));
}

}  // namespace blink
