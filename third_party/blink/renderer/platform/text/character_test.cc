// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/character.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

testing::AssertionResult IsCJKIdeographOrSymbolWithMessage(UChar32 codepoint) {
  const size_t kFormatBufferSize = 10;
  char formatted_as_hex[kFormatBufferSize];
  snprintf(formatted_as_hex, kFormatBufferSize, "0x%x", codepoint);

  if (Character::IsCJKIdeographOrSymbol(codepoint)) {
    return testing::AssertionSuccess()
           << "Codepoint " << formatted_as_hex << " is a CJKIdeographOrSymbol.";
  }

  return testing::AssertionFailure() << "Codepoint " << formatted_as_hex
                                     << " is not a CJKIdeographOrSymbol.";
}

TEST(CharacterTest, HammerEmojiVsCJKIdeographOrSymbol) {
  for (UChar32 test_char = 0; test_char < kMaxCodepoint; test_char++) {
    if (Character::IsEmojiEmojiDefault(test_char)) {
      EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(test_char));
    }
  }
}

static void TestSpecificUChar32RangeIdeograph(UChar32 range_start,
                                              UChar32 range_end,
                                              bool before = true,
                                              bool after = true) {
  if (before) {
    EXPECT_FALSE(Character::IsCJKIdeographOrSymbol(range_start - 1))
        << std::hex << (range_start - 1);
  }
  EXPECT_TRUE(Character::IsCJKIdeographOrSymbol(range_start))
      << std::hex << range_start;
  UChar32 mid = static_cast<UChar32>(
      (static_cast<uint64_t>(range_start) + range_end) / 2);
  EXPECT_TRUE(Character::IsCJKIdeographOrSymbol(mid)) << std::hex << mid;
  EXPECT_TRUE(Character::IsCJKIdeographOrSymbol(range_end))
      << std::hex << range_end;
  if (after) {
    EXPECT_FALSE(Character::IsCJKIdeographOrSymbol(range_end + 1))
        << std::hex << (range_end + 1);
  }
}

TEST(CharacterTest, TestIsCJKIdeograph) {
  // The basic CJK Unified Ideographs block.
  TestSpecificUChar32RangeIdeograph(0x4E00, 0x9FFF, false);
  // CJK Unified Ideographs Extension A.
  TestSpecificUChar32RangeIdeograph(0x3400, 0x4DBF, false, false);
  // CJK Unified Ideographs Extension A and Kangxi Radicals.
  TestSpecificUChar32RangeIdeograph(0x2E80, 0x2FDF);
  // CJK Strokes.
  TestSpecificUChar32RangeIdeograph(0x31C0, 0x31EF, false);
  // CJK Compatibility Ideographs.
  TestSpecificUChar32RangeIdeograph(0xF900, 0xFAFF);
  // CJK Unified Ideographs Extension B.
  TestSpecificUChar32RangeIdeograph(0x20000, 0x2A6DF, true, false);
  // CJK Unified Ideographs Extension C.
  // CJK Unified Ideographs Extension D.
  TestSpecificUChar32RangeIdeograph(0x2A700, 0x2B81F, false, false);
  // CJK Compatibility Ideographs Supplement.
  TestSpecificUChar32RangeIdeograph(0x2F800, 0x2FA1F, false, false);
}

static void TestSpecificUChar32RangeIdeographSymbol(UChar32 range_start,
                                                    UChar32 range_end) {
  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(range_start - 1));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(range_start));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(
      (UChar32)((uint64_t)range_start + (uint64_t)range_end) / 2));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(range_end));
  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(range_end + 1));
}

TEST(CharacterTest, TestIsCJKIdeographOrSymbol) {
  // CJK Compatibility Ideographs Supplement.
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2C7));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2CA));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2CB));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2D9));

  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2020));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2021));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2030));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x203B));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x203C));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2042));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2047));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2048));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2049));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2051));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x20DD));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x20DE));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2100));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2103));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2105));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2109));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x210A));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2113));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2116));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2121));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x212B));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x213B));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2150));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2151));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2152));

  TestSpecificUChar32RangeIdeographSymbol(0x2156, 0x215A);
  TestSpecificUChar32RangeIdeographSymbol(0x2160, 0x216B);
  TestSpecificUChar32RangeIdeographSymbol(0x2170, 0x217B);

  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x217F));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2189));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2307));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2312));

  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(0x23BD));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x23BE));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x23C4));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x23CC));
  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(0x23CD));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x23CE));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2423));

  TestSpecificUChar32RangeIdeographSymbol(0x2460, 0x2492);
  TestSpecificUChar32RangeIdeographSymbol(0x249C, 0x24FF);

  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25A0));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25A1));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25A2));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25AA));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25AB));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25B1));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25B2));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25B3));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25B6));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25B7));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25BC));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25BD));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25C0));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25C1));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25C6));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25C7));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25C9));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25CB));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25CC));

  TestSpecificUChar32RangeIdeographSymbol(0x25CE, 0x25D3);
  TestSpecificUChar32RangeIdeographSymbol(0x25E2, 0x25E6);

  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x25EF));

  TestSpecificUChar32RangeIdeographSymbol(0x2600, 0x2603);

  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2605));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2606));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x260E));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2616));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2617));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2640));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2642));

  TestSpecificUChar32RangeIdeographSymbol(0x2660, 0x266F);
  TestSpecificUChar32RangeIdeographSymbol(0x2672, 0x267D);

  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x26A0));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x26BD));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x26BE));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2713));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x271A));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x273F));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2740));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2756));

  TestSpecificUChar32RangeIdeographSymbol(0x2763, 0x2764);
  TestSpecificUChar32RangeIdeographSymbol(0x2776, 0x277F);

  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x2B1A));

  TestSpecificUChar32RangeIdeographSymbol(0x2FF0, 0x302D);
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x3031));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x312F));
  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(0x3130));

  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(0x318F));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x3190));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x319F));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x31BF));

  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(0x31FF));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x3200));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x3300));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x33FF));

  TestSpecificUChar32RangeIdeographSymbol(0xF860, 0xF862);
  TestSpecificUChar32RangeIdeographSymbol(0xFE30, 0xFE6F);

  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0xFE10));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0xFE11));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0xFE12));
  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0xFE19));

  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(0xFF0D));
  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(0xFF1B));
  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(0xFF1C));
  EXPECT_FALSE(IsCJKIdeographOrSymbolWithMessage(0xFF1E));

  TestSpecificUChar32RangeIdeographSymbol(0xFF00, 0xFFEF);

  EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(0x1F100));

  TestSpecificUChar32RangeIdeographSymbol(0x1F110, 0x1F129);
  TestSpecificUChar32RangeIdeographSymbol(0x1F130, 0x1F149);
  TestSpecificUChar32RangeIdeographSymbol(0x1F150, 0x1F169);
  TestSpecificUChar32RangeIdeographSymbol(0x1F170, 0x1F189);
  TestSpecificUChar32RangeIdeographSymbol(0x1F1E6, 0x1F6FF);
}

TEST(CharacterTest, CanTextDecorationSkipInk) {
  // ASCII
  EXPECT_TRUE(Character::CanTextDecorationSkipInk('a'));
  // Hangul Jamo
  EXPECT_FALSE(Character::CanTextDecorationSkipInk(0x1100));
  // Hiragana
  EXPECT_FALSE(Character::CanTextDecorationSkipInk(0x3041));
  // Bopomofo
  EXPECT_FALSE(Character::CanTextDecorationSkipInk(0x31A0));
  // The basic CJK Unified Ideographs block
  EXPECT_FALSE(Character::CanTextDecorationSkipInk(0x4E01));
  // Hangul Syllables
  EXPECT_FALSE(Character::CanTextDecorationSkipInk(0xAC00));
  // Plane 2 / CJK Ideograph Extension B
  EXPECT_FALSE(Character::CanTextDecorationSkipInk(0x20000));
}

TEST(CharacterTest, TestEmojiTextDefault) {
  // Text-default emoji, i.e.
  // Emoji=Yes and EmojiPresentation=No
  EXPECT_TRUE(Character::IsEmojiTextDefault(0x0023));
  EXPECT_TRUE(Character::IsEmojiTextDefault(0x2744));
  EXPECT_TRUE(Character::IsEmojiTextDefault(0x1F6F3));

  // Non-emoji
  EXPECT_FALSE(Character::IsEmojiTextDefault('A'));
  EXPECT_FALSE(Character::IsEmojiTextDefault(0x2713));

  // Emoji=Yes and EmojiPresentation=Yes
  EXPECT_FALSE(Character::IsEmojiTextDefault(0x1F9C0));
  EXPECT_FALSE(Character::IsEmojiTextDefault(0x26BD));
  EXPECT_FALSE(Character::IsEmojiTextDefault(0x26BE));
}

TEST(CharacterTest, TestEmojiEmojiDefault) {
  // Emoji=Yes and EmojiPresentation=Yes
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x231A));
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x1F191));
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x1F19A));
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x1F9C0));
  // Kiss
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x1F48F));
  // Couple with heart
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x1F491));
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x1F46A));

  // Non-emoji
  EXPECT_FALSE(Character::IsEmojiEmojiDefault('A'));

  // Emoji=Yes and EmojiPresentation=No
  EXPECT_FALSE(Character::IsEmojiEmojiDefault(0x1F202));
}

TEST(CharacterTest, EmojificationV11) {
  // Infinity and Chess pawn were given the emoji class, but have default text
  // presentation in Unicode 11.
  EXPECT_TRUE(Character::IsEmojiTextDefault(0x265F));
  EXPECT_TRUE(Character::IsEmojiTextDefault(0x267E));
}

TEST(CharacterTest, TestEmojiModifierBase) {
  EXPECT_TRUE(Character::IsEmojiModifierBase(0x261D));
  EXPECT_TRUE(Character::IsEmojiModifierBase(0x1F470));
  EXPECT_TRUE(Character::IsEmojiModifierBase(0x1F478));
  EXPECT_TRUE(Character::IsEmojiModifierBase(0x1F918));
  EXPECT_FALSE(Character::IsEmojiModifierBase('A'));
  EXPECT_FALSE(Character::IsEmojiModifierBase(0x1F47D));
}

TEST(CharacterTest, TestEmoji40Data) {
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x1F32F));
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x1F57A));
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x1F919));
  EXPECT_TRUE(Character::IsEmojiEmojiDefault(0x1F926));
  EXPECT_TRUE(Character::IsEmojiModifierBase(0x1F574));
  EXPECT_TRUE(Character::IsEmojiModifierBase(0x1F6CC));
  EXPECT_TRUE(Character::IsEmojiModifierBase(0x1F919));
  EXPECT_TRUE(Character::IsEmojiModifierBase(0x1F926));
  EXPECT_TRUE(Character::IsEmojiModifierBase(0x1F933));
}

TEST(CharacterTest, LineBreakAndQuoteNotEmoji) {
  EXPECT_FALSE(Character::IsEmojiTextDefault('\n'));
  EXPECT_FALSE(Character::IsEmojiTextDefault('"'));
}

TEST(CharacterTest, Truncation) {
  const UChar32 kBase = 0x90000;
  UChar32 test_char = 0;

  test_char = kBase + kSpaceCharacter;
  EXPECT_FALSE(Character::TreatAsSpace(test_char));
  test_char = kBase + kNoBreakSpaceCharacter;
  EXPECT_FALSE(Character::TreatAsSpace(test_char));

  test_char = kBase + kZeroWidthNonJoinerCharacter;
  EXPECT_FALSE(Character::TreatAsZeroWidthSpace(test_char));
  test_char = kBase + kZeroWidthJoinerCharacter;
  EXPECT_FALSE(Character::TreatAsZeroWidthSpace(test_char));

  test_char = kBase + 0x12;
  EXPECT_FALSE(Character::TreatAsZeroWidthSpaceInComplexScript(test_char));
  EXPECT_FALSE(Character::TreatAsZeroWidthSpaceInComplexScript(test_char));
  test_char = kBase + kObjectReplacementCharacter;
  EXPECT_FALSE(Character::TreatAsZeroWidthSpaceInComplexScript(test_char));

  test_char = kBase + 0xA;
  EXPECT_FALSE(Character::IsNormalizedCanvasSpaceCharacter(test_char));
  test_char = kBase + 0x9;
  EXPECT_FALSE(Character::IsNormalizedCanvasSpaceCharacter(test_char));
}

TEST(CharacterTest, IsBidiControl) {
  EXPECT_TRUE(Character::IsBidiControl(0x202A));  // LEFT-TO-RIGHT EMBEDDING
  EXPECT_TRUE(Character::IsBidiControl(0x202B));  // RIGHT-TO-LEFT EMBEDDING
  EXPECT_TRUE(Character::IsBidiControl(0x202D));  // LEFT-TO-RIGHT OVERRIDE
  EXPECT_TRUE(Character::IsBidiControl(0x202E));  // RIGHT-TO-LEFT OVERRIDE
  EXPECT_TRUE(Character::IsBidiControl(0x202C));  // POP DIRECTIONAL FORMATTING
  EXPECT_TRUE(Character::IsBidiControl(0x2066));  // LEFT-TO-RIGHT ISOLATE
  EXPECT_TRUE(Character::IsBidiControl(0x2067));  // RIGHT-TO-LEFT ISOLATE
  EXPECT_TRUE(Character::IsBidiControl(0x2068));  // FIRST STRONG ISOLATE
  EXPECT_TRUE(Character::IsBidiControl(0x2069));  // POP DIRECTIONAL ISOLATE
  EXPECT_TRUE(Character::IsBidiControl(0x200E));  // LEFT-TO-RIGHT MARK
  EXPECT_TRUE(Character::IsBidiControl(0x200F));  // RIGHT-TO-LEFT MARK
  EXPECT_TRUE(Character::IsBidiControl(0x061C));  // ARABIC LETTER MARK
  EXPECT_FALSE(Character::IsBidiControl('A'));
  EXPECT_FALSE(Character::IsBidiControl('0'));
  EXPECT_FALSE(Character::IsBidiControl(0x05D0));
}

TEST(CharacterTest, IsNonCharacter) {
  // See http://www.unicode.org/faq/private_use.html#nonchar4
  EXPECT_FALSE(Character::IsNonCharacter(0xFDD0 - 1));
  for (UChar32 bmp_noncharacter = 0xFDD0; bmp_noncharacter < 0xFDEF;
       ++bmp_noncharacter) {
    EXPECT_TRUE(Character::IsNonCharacter(bmp_noncharacter));
  }
  EXPECT_FALSE(Character::IsNonCharacter(0xFDEF + 1));

  EXPECT_FALSE(Character::IsNonCharacter(0xFFFE - 1));
  EXPECT_TRUE(Character::IsNonCharacter(0xFFFE));
  EXPECT_TRUE(Character::IsNonCharacter(0xFFFF));
  EXPECT_FALSE(Character::IsNonCharacter(0xFFFF + 1));

  for (uint32_t supplementary_plane_prefix = 0x10000;
       supplementary_plane_prefix < 0x100000;
       supplementary_plane_prefix += 0x10000) {
    EXPECT_FALSE(
        Character::IsNonCharacter(supplementary_plane_prefix + 0xFFFE - 1));
    EXPECT_TRUE(Character::IsNonCharacter(supplementary_plane_prefix + 0xFFFE));
    EXPECT_TRUE(Character::IsNonCharacter(supplementary_plane_prefix + 0xFFFF));
    EXPECT_FALSE(
        Character::IsNonCharacter(supplementary_plane_prefix + 0xFFFF + 1));
  }
}

TEST(CharacterTest, TransformedIsUprightInMixedVertical) {
  // Compare
  // https://unicode.org/cldr/utility/list-unicodeset.jsp?a=%5B%3AVertical_Orientation%3DTransformed_Upright%3A%5D&g=&i=
  const UChar32 vertical_orientation_transformed_upright_category[] = {
      0x3001, 0x3002,  0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083,
      0x3085, 0x3087,  0x308E, 0x3095, 0x3096, 0x309B, 0x309C, 0x30A1, 0x30A3,
      0x30A5, 0x30A7,  0x30A9, 0x30C3, 0x30E3, 0x30E5, 0x30E7, 0x30EE, 0x30F5,
      0x30F6, 0x3127,  0x31F0, 0x31F1, 0x31F2, 0x31F3, 0x31F4, 0x31F5, 0x31F6,
      0x31F7, 0x31F8,  0x31F9, 0x31FA, 0x31FB, 0x31FC, 0x31FD, 0x31FE, 0x31FF,
      0x3300, 0x3301,  0x3302, 0x3303, 0x3304, 0x3305, 0x3306, 0x3307, 0x3308,
      0x3309, 0x330A,  0x330B, 0x330C, 0x330D, 0x330E, 0x330F, 0x3310, 0x3311,
      0x3312, 0x3313,  0x3314, 0x3315, 0x3316, 0x3317, 0x3318, 0x3319, 0x331A,
      0x331B, 0x331C,  0x331D, 0x331E, 0x331F, 0x3320, 0x3321, 0x3322, 0x3323,
      0x3324, 0x3325,  0x3326, 0x3327, 0x3328, 0x3329, 0x332A, 0x332B, 0x332C,
      0x332D, 0x332E,  0x332F, 0x3330, 0x3331, 0x3332, 0x3333, 0x3334, 0x3335,
      0x3336, 0x3337,  0x3338, 0x3339, 0x333A, 0x333B, 0x333C, 0x333D, 0x333E,
      0x333F, 0x3340,  0x3341, 0x3342, 0x3343, 0x3344, 0x3345, 0x3346, 0x3347,
      0x3348, 0x3349,  0x334A, 0x334B, 0x334C, 0x334D, 0x334E, 0x334F, 0x3350,
      0x3351, 0x3352,  0x3353, 0x3354, 0x3355, 0x3356, 0x3357, 0x337B, 0x337C,
      0x337D, 0x337E,  0x337F, 0xFE50, 0xFE51, 0xFE52, 0xFF01, 0xFF0C, 0xFF0E,
      0xFF1F, 0x1F200, 0x1F201};

  for (UChar32 transformed_upright_character :
       vertical_orientation_transformed_upright_category) {
    EXPECT_TRUE(
        Character::IsUprightInMixedVertical(transformed_upright_character));
  }
}

}  // namespace blink
