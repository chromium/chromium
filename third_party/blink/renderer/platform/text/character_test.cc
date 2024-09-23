// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/text/character.h"

#include <ubidi_props.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/text/emoji_segmentation_category.h"
#include "third_party/blink/renderer/platform/text/emoji_segmentation_category_inline_header.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

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

// Test Unicode-derived functions work as intended.
// These functions may need to be adjusted if Unicode changes.
TEST(CharacterTest, Derived) {
  StringBuilder builder;
  for (UChar32 ch = 0; ch < kMaxCodepoint; ++ch) {
    if (Character::IsEmojiEmojiDefault(ch)) {
      EXPECT_TRUE(IsCJKIdeographOrSymbolWithMessage(ch));
    }

    const UBlockCode block = ublock_getCode(ch);
    EXPECT_EQ(Character::IsBlockCjkSymbolsAndPunctuation(ch),
              block == UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION);
    EXPECT_EQ(Character::IsBlockHalfwidthAndFullwidthForms(ch),
              block == UBLOCK_HALFWIDTH_AND_FULLWIDTH_FORMS);

    const UEastAsianWidth eaw = Character::EastAsianWidth(ch);
    EXPECT_EQ(Character::IsEastAsianWidthFullwidth(ch),
              eaw == UEastAsianWidth::U_EA_FULLWIDTH);

    if (!Character::MaybeHanKerningOpenOrCloseFast(ch)) {
      DCHECK(!Character::MaybeHanKerningOpenSlow(ch));
      DCHECK(!Character::MaybeHanKerningCloseSlow(ch));
    }

    // Test UTF-16 functions.
    const UCharDirection bidi = ubidi_getClass(ch);
    if (bidi == UCharDirection::U_RIGHT_TO_LEFT ||
        bidi == UCharDirection::U_RIGHT_TO_LEFT_ARABIC ||
        Character::IsBidiControl(ch)) {
      builder.Clear();
      builder.Append(ch);
      const String utf16 = builder.ToString();
      DCHECK(Character::MaybeBidiRtl(utf16));
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

TEST(CharacterTest, HanKerning) {
  struct Data {
    UChar32 ch;
    HanKerningCharType type;
  } data_list[] = {
      {kLeftDoubleQuotationMarkCharacter, HanKerningCharType::kOpenQuote},
      {kRightDoubleQuotationMarkCharacter, HanKerningCharType::kCloseQuote},
      {kMiddleDotCharacter, HanKerningCharType::kMiddle},
      {kIdeographicSpaceCharacter, HanKerningCharType::kMiddle},
      {kFullwidthComma, HanKerningCharType::kDot},
      {0x3008, HanKerningCharType::kOpen},
      {0xFF5F, HanKerningCharType::kOpen},
      {0x3009, HanKerningCharType::kClose},
      {0xFF60, HanKerningCharType::kClose},
      {0x0028, HanKerningCharType::kOpenNarrow},
      {0xFF62, HanKerningCharType::kOpenNarrow},
      {0x0029, HanKerningCharType::kCloseNarrow},
      {0xFF63, HanKerningCharType::kCloseNarrow},
  };
  for (const Data& data : data_list) {
    EXPECT_EQ(Character::GetHanKerningCharType(data.ch), data.type);
  }
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

TEST(CharacterTest, IsVerticalMathCharacter) {
  // https://w3c.github.io/mathml-core/#stretchy-operator-axis
  const UChar stretchy_operator_with_inline_axis[]{
      0x003D, 0x005E, 0x005F, 0x007E, 0x00AF, 0x02C6, 0x02C7, 0x02C9, 0x02CD,
      0x02DC, 0x02F7, 0x0302, 0x0332, 0x203E, 0x20D0, 0x20D1, 0x20D6, 0x20D7,
      0x20E1, 0x2190, 0x2192, 0x2194, 0x2198, 0x2199, 0x219C, 0x219D, 0x219E,
      0x21A0, 0x21A2, 0x21A3, 0x21A4, 0x21A6, 0x21A9, 0x21AA, 0x21AB, 0x21AC,
      0x21AD, 0x21B4, 0x21B9, 0x21BC, 0x21BD, 0x21C0, 0x21C1, 0x21C4, 0x21C6,
      0x21C7, 0x21C9, 0x21CB, 0x21CC, 0x21D0, 0x21D2, 0x21D4, 0x21DA, 0x21DB,
      0x21DC, 0x21DD, 0x21E0, 0x21E2, 0x21E4, 0x21E5, 0x21E6, 0x21E8, 0x21F0,
      0x21F6, 0x21FD, 0x21FE, 0x21FF, 0x23B4, 0x23B5, 0x23DC, 0x23DD, 0x23DE,
      0x23DF, 0x23E0, 0x23E1, 0x2500, 0x27F5, 0x27F6, 0x27F7, 0x27F8, 0x27F9,
      0x27FA, 0x27FB, 0x27FC, 0x27FD, 0x27FE, 0x27FF, 0x290C, 0x290D, 0x290E,
      0x290F, 0x2910, 0x294E, 0x2950, 0x2952, 0x2953, 0x2956, 0x2957, 0x295A,
      0x295B, 0x295E, 0x295F, 0x2B45, 0x2B46, 0xFE35, 0xFE36, 0xFE37, 0xFE38};

  for (UChar32 test_char = 0; test_char < kMaxCodepoint; test_char++) {
    if (test_char == kArabicMathematicalOperatorMeemWithHahWithTatweel) {
      EXPECT_FALSE(Character::IsVerticalMathCharacter(test_char));
    } else if (test_char == kArabicMathematicalOperatorHahWithDal) {
      EXPECT_FALSE(Character::IsVerticalMathCharacter(test_char));
    } else {
      bool in_vertical =
          !std::binary_search(stretchy_operator_with_inline_axis,
                              stretchy_operator_with_inline_axis +
                                  std::size(stretchy_operator_with_inline_axis),
                              test_char);
      EXPECT_TRUE(Character::IsVerticalMathCharacter(test_char) == in_vertical);
    }
  }
}

TEST(CharacterTest, ExtendedPictographic) {
  EXPECT_FALSE(Character::IsExtendedPictographic(0x00A8));
  EXPECT_TRUE(Character::IsExtendedPictographic(0x00A9));
  EXPECT_FALSE(Character::IsExtendedPictographic(0x00AA));
  EXPECT_FALSE(Character::IsExtendedPictographic(0x3298));
  EXPECT_TRUE(Character::IsExtendedPictographic(0x3299));
  EXPECT_FALSE(Character::IsExtendedPictographic(0x329A));
}

TEST(CharacterTest, EmojiComponents) {
  UChar32 false_set[] = {0x22,    0x2B,    0x29,    0x40,    0x200C,  0x200E,
                         0x20E2,  0x20E4,  0xFE0E,  0xFE1A,  0x1F1E5, 0x1f200,
                         0x1f3fa, 0x1f400, 0x1f9Af, 0x1f9b4, 0xe001F, 0xe0080};
  UChar32 true_set[] = {0x23,    0x2a,    0x30,    0x39,    0x200d,
                        0x20e3,  0xfe0f,  0x1f1e6, 0x1f1ff, 0x1f3fb,
                        0x1f3ff, 0x1f9b0, 0x1f9b3, 0xe0020, 0xe007f};

  for (auto false_test : false_set)
    EXPECT_FALSE(Character::IsEmojiComponent(false_test));

  for (auto true_test : true_set)
    EXPECT_TRUE(Character::IsEmojiComponent(true_test));
}

// Ensure that the iterator forwarding in SymbolsIterator is not
// skipping any other categories that would be computed for the same cursor
// position and codepoint.
TEST(CharacterTest, MaybeEmojiPresentationNoIllegalShortcut) {
  for (UChar32 ch = 0; ch < kMaxCodepoint; ++ch) {
    const EmojiSegmentationCategory emoji = GetEmojiSegmentationCategory(ch);
    if (IsEmojiPresentationCategory(emoji)) {
      EXPECT_TRUE(Character::MaybeEmojiPresentation(ch));
    }
    if (!Character::MaybeEmojiPresentation(ch)) {
      EXPECT_FALSE(IsEmojiPresentationCategory(emoji));
    }
  }
}

TEST(CharacterTest, TestIsStandardizedVariationSequence) {
  EXPECT_TRUE(Character::IsStandardizedVariationSequence(0x2293, 0xfe00));
  EXPECT_TRUE(Character::IsStandardizedVariationSequence(0x8279, 0xfe00));
  EXPECT_TRUE(Character::IsStandardizedVariationSequence(0x8279, 0xfe01));
  EXPECT_FALSE(Character::IsStandardizedVariationSequence(0x8279, 0xe0100));
  EXPECT_FALSE(Character::IsStandardizedVariationSequence(0x8279, 0xfe03));
}

TEST(CharacterTest, TestIsEmojiVariationSequence) {
  EXPECT_TRUE(Character::IsEmojiVariationSequence(0x1fae8, 0xfe0f));
  EXPECT_TRUE(Character::IsEmojiVariationSequence(0x0030, 0xfe0e));
  EXPECT_FALSE(Character::IsEmojiVariationSequence(0x1faf0, 0xfe00));
  EXPECT_FALSE(Character::IsEmojiVariationSequence(0x0041, 0xfe0f));
}

TEST(CharacterTest, TestIsIdeographicVariationSequence) {
  EXPECT_TRUE(Character::IsIdeographicVariationSequence(0x8279, 0xe0100));
  EXPECT_TRUE(Character::IsIdeographicVariationSequence(0x8279, 0xe01ef));
  EXPECT_TRUE(Character::IsIdeographicVariationSequence(0x9038, 0xe0101));
  EXPECT_TRUE(Character::IsIdeographicVariationSequence(0x9038, 0xe01ef));
  EXPECT_FALSE(Character::IsIdeographicVariationSequence(0x9038, 0xfe00));
  EXPECT_FALSE(Character::IsIdeographicVariationSequence(0x0041, 0xe0100));
}

}  // namespace blink
