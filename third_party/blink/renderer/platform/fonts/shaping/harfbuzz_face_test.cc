// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"

#include "hb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/glyph.h"
#include "third_party/blink/renderer/platform/fonts/shaping/variation_selector_mode.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

namespace {

String WPTFontPath(const String& font_name) {
  return test::BlinkWebTestsDir() +
         "/external/wpt/css/css-fonts/resources/vs/" + font_name;
}

hb_codepoint_t GetGlyphForVariationSequenceFromFont(
    Font font,
    UChar32 character,
    UChar32 variation_selector) {
  const FontPlatformData& font_without_char_platform_data =
      font.PrimaryFont()->PlatformData();
  HarfBuzzFace* face_without_char =
      font_without_char_platform_data.GetHarfBuzzFace();
  EXPECT_TRUE(face_without_char);
  return face_without_char->HarfBuzzGetGlyphForTesting(character,
                                                       variation_selector);
}

hb_codepoint_t GetGlyphForEmojiVSFromFontWithVS15(UChar32 character,
                                                  UChar32 variation_selector) {
  Font font =
      test::CreateTestFont(AtomicString("Noto Emoji"),
                           WPTFontPath("NotoEmoji-Regular_subset.ttf"), 11);
  return GetGlyphForVariationSequenceFromFont(font, character,
                                              variation_selector);
}

hb_codepoint_t GetGlyphForEmojiVSFromFontWithVS16(UChar32 character,
                                                  UChar32 variation_selector) {
  Font font = test::CreateTestFont(
      AtomicString("Noto Color Emoji"),
      WPTFontPath("NotoColorEmoji-Regular_subset.ttf"), 11);
  return GetGlyphForVariationSequenceFromFont(font, character,
                                              variation_selector);
}

hb_codepoint_t GetGlyphForEmojiVSFromFontWithBaseCharOnly(
    UChar32 character,
    UChar32 variation_selector) {
  Font font = test::CreateTestFont(
      AtomicString("Noto Emoji Without VS"),
      WPTFontPath("NotoEmoji-Regular_without-cmap14-subset.ttf"), 11);
  return GetGlyphForVariationSequenceFromFont(font, character,
                                              variation_selector);
}

hb_codepoint_t GetGlyphForStandardizedVSFromFontWithBaseCharOnly() {
  UChar32 character = kMongolianLetterA;
  UChar32 variation_selector = kMongolianFreeVariationSelectorTwo;

  Font font = test::CreateTestFont(AtomicString("Noto Sans Mongolian"),
                                   blink::test::BlinkWebTestsFontsTestDataPath(
                                       "noto/NotoSansMongolian-regular.woff2"),
                                   11);
  return GetGlyphForVariationSequenceFromFont(font, character,
                                              variation_selector);
}

hb_codepoint_t GetGlyphForCJKVSFromFontWithVS() {
  UChar32 character = kFullwidthExclamationMark;
  UChar32 variation_selector = kVariationSelector2Character;

  Font font = test::CreateTestFont(
      AtomicString("Noto Sans CJK JP"),
      blink::test::BlinkWebTestsFontsTestDataPath(
          "noto/cjk/NotoSansCJKjp-Regular-subset-chws.otf"),
      11);
  return GetGlyphForVariationSequenceFromFont(font, character,
                                              variation_selector);
}

}  // namespace

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestFontWithVS) {
  ScopedFontVariationSequencesForTest scoped_feature(true);
  HarfBuzzFace::SetVariationSelectorMode(kUseSpecifiedVariationSelector);

  hb_codepoint_t glyph = GetGlyphForCJKVSFromFontWithVS();
  EXPECT_TRUE(glyph);
  EXPECT_NE(glyph, kUnmatchedVSGlyphId);
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestFontWithVS_IgnoreVS) {
  ScopedFontVariationSequencesForTest scoped_feature(true);
  HarfBuzzFace::SetVariationSelectorMode(kIgnoreVariationSelector);

  hb_codepoint_t glyph = GetGlyphForCJKVSFromFontWithVS();
  EXPECT_TRUE(glyph);
  EXPECT_NE(glyph, kUnmatchedVSGlyphId);
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestFontWithVS_VSFlagOff) {
  ScopedFontVariationSequencesForTest scoped_feature(false);
  HarfBuzzFace::SetVariationSelectorMode(kUseSpecifiedVariationSelector);

  hb_codepoint_t glyph = GetGlyphForCJKVSFromFontWithVS();
  EXPECT_TRUE(glyph);
  EXPECT_NE(glyph, kUnmatchedVSGlyphId);
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestFontWithBaseCharOnly) {
  ScopedFontVariationSequencesForTest scoped_feature(true);
  HarfBuzzFace::SetVariationSelectorMode(kUseSpecifiedVariationSelector);

  EXPECT_EQ(GetGlyphForStandardizedVSFromFontWithBaseCharOnly(),
            kUnmatchedVSGlyphId);
}

TEST(HarfBuzzFaceTest,
     HarfBuzzGetNominalGlyph_TestFontWithBaseCharOnly_IgnoreVS) {
  ScopedFontVariationSequencesForTest scoped_feature(true);
  HarfBuzzFace::SetVariationSelectorMode(kIgnoreVariationSelector);

  hb_codepoint_t glyph = GetGlyphForStandardizedVSFromFontWithBaseCharOnly();
  EXPECT_FALSE(glyph);
}

TEST(HarfBuzzFaceTest,
     HarfBuzzGetNominalGlyph_TestFontWithBaseCharOnly_VSFlagOff) {
  ScopedFontVariationSequencesForTest scoped_feature(false);
  HarfBuzzFace::SetVariationSelectorMode(kUseSpecifiedVariationSelector);

  hb_codepoint_t glyph = GetGlyphForStandardizedVSFromFontWithBaseCharOnly();
  EXPECT_FALSE(glyph);
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestFontWithoutBaseChar) {
  ScopedFontVariationSequencesForTest scoped_feature(true);
  HarfBuzzFace::SetVariationSelectorMode(kUseSpecifiedVariationSelector);

  UChar32 character = kFullwidthExclamationMark;
  UChar32 variation_selector = kVariationSelector2Character;

  Font font = test::CreateAhemFont(11);
  EXPECT_FALSE(GetGlyphForVariationSequenceFromFont(font, character,
                                                    variation_selector));
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestVariantEmojiEmoji) {
  ScopedFontVariationSequencesForTest scoped_variation_sequences_feature(true);
  ScopedFontVariantEmojiForTest scoped_variant_emoji_feature(true);

  HarfBuzzFace::SetVariationSelectorMode(kForceVariationSelector16);

  UChar32 character = kShakingFaceEmoji;
  UChar32 variation_selector = 0;

  hb_codepoint_t glyph_from_font_with_vs15 =
      GetGlyphForEmojiVSFromFontWithVS15(character, variation_selector);
  EXPECT_EQ(glyph_from_font_with_vs15, kUnmatchedVSGlyphId);

  hb_codepoint_t glyph_from_font_with_vs16 =
      GetGlyphForEmojiVSFromFontWithVS16(character, variation_selector);
  EXPECT_TRUE(glyph_from_font_with_vs16);
  EXPECT_NE(glyph_from_font_with_vs16, kUnmatchedVSGlyphId);

  hb_codepoint_t glyph_from_font_without_vs =
      GetGlyphForEmojiVSFromFontWithBaseCharOnly(character, variation_selector);
  EXPECT_EQ(glyph_from_font_without_vs, kUnmatchedVSGlyphId);
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestVariantEmojiText) {
  ScopedFontVariationSequencesForTest scoped_variation_sequences_feature(true);
  ScopedFontVariantEmojiForTest scoped_variant_emoji_feature(true);

  HarfBuzzFace::SetVariationSelectorMode(kForceVariationSelector15);

  UChar32 character = kShakingFaceEmoji;
  UChar32 variation_selector = 0;

  hb_codepoint_t glyph_from_font_with_vs15 =
      GetGlyphForEmojiVSFromFontWithVS15(character, variation_selector);
  EXPECT_TRUE(glyph_from_font_with_vs15);
  EXPECT_NE(glyph_from_font_with_vs15, kUnmatchedVSGlyphId);

  hb_codepoint_t glyph_from_font_with_vs16 =
      GetGlyphForEmojiVSFromFontWithVS16(character, variation_selector);
  EXPECT_EQ(glyph_from_font_with_vs16, kUnmatchedVSGlyphId);

  hb_codepoint_t glyph_from_font_without_vs =
      GetGlyphForEmojiVSFromFontWithBaseCharOnly(character, variation_selector);
  EXPECT_EQ(glyph_from_font_without_vs, kUnmatchedVSGlyphId);
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestVariantEmojiUnicode) {
  ScopedFontVariationSequencesForTest scoped_variation_sequences_feature(true);
  ScopedFontVariantEmojiForTest scoped_variant_emoji_feature(true);

  HarfBuzzFace::SetVariationSelectorMode(kUseUnicodeDefaultPresentation);

  UChar32 character = kShakingFaceEmoji;
  UChar32 variation_selector = 0;

  hb_codepoint_t glyph_from_font_with_vs15 =
      GetGlyphForEmojiVSFromFontWithVS15(character, variation_selector);
  EXPECT_EQ(glyph_from_font_with_vs15, kUnmatchedVSGlyphId);

  hb_codepoint_t glyph_from_font_with_vs16 =
      GetGlyphForEmojiVSFromFontWithVS16(character, variation_selector);
  EXPECT_TRUE(glyph_from_font_with_vs16);
  EXPECT_NE(glyph_from_font_with_vs16, kUnmatchedVSGlyphId);

  hb_codepoint_t glyph_from_font_without_vs =
      GetGlyphForEmojiVSFromFontWithBaseCharOnly(character, variation_selector);
  EXPECT_EQ(glyph_from_font_without_vs, kUnmatchedVSGlyphId);
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestVSOverrideVariantEmoji) {
  ScopedFontVariationSequencesForTest scoped_variation_sequences_feature(true);
  ScopedFontVariantEmojiForTest scoped_variant_emoji_feature(true);

  HarfBuzzFace::SetVariationSelectorMode(kForceVariationSelector16);

  UChar32 character = kShakingFaceEmoji;
  UChar32 variation_selector = kVariationSelector15Character;

  hb_codepoint_t glyph_from_font_with_vs15 =
      GetGlyphForEmojiVSFromFontWithVS15(character, variation_selector);
  EXPECT_TRUE(glyph_from_font_with_vs15);
  EXPECT_NE(glyph_from_font_with_vs15, kUnmatchedVSGlyphId);

  hb_codepoint_t glyph_from_font_with_vs16 =
      GetGlyphForEmojiVSFromFontWithVS16(character, variation_selector);
  EXPECT_EQ(glyph_from_font_with_vs16, kUnmatchedVSGlyphId);

  hb_codepoint_t glyph_from_font_without_vs =
      GetGlyphForEmojiVSFromFontWithBaseCharOnly(character, variation_selector);
  EXPECT_EQ(glyph_from_font_without_vs, kUnmatchedVSGlyphId);
}

// Test emoji variation selectors support in system fallback. We are only
// enabling this feature on Windows, Android and Mac platforms.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestSystemFallbackEmojiVS) {
  ScopedFontVariationSequencesForTest scoped_variation_sequences_feature(true);
  ScopedFontVariantEmojiForTest scoped_variant_emoji_feature(true);
  ScopedSystemFallbackEmojiVSSupportForTest scoped_system_emoji_vs_feature(
      true);

  HarfBuzzFace::SetVariationSelectorMode(kUseSpecifiedVariationSelector);
  HarfBuzzFace::SetIsSystemFallbackStage(true);

  UChar32 character = kShakingFaceEmoji;

  hb_codepoint_t glyph_from_font_with_vs15 = GetGlyphForEmojiVSFromFontWithVS15(
      character, kVariationSelector15Character);
  EXPECT_TRUE(glyph_from_font_with_vs15);
  EXPECT_NE(glyph_from_font_with_vs15, kUnmatchedVSGlyphId);

  hb_codepoint_t glyph_from_font_with_vs16 = GetGlyphForEmojiVSFromFontWithVS16(
      character, kVariationSelector16Character);
  EXPECT_TRUE(glyph_from_font_with_vs16);
  EXPECT_NE(glyph_from_font_with_vs16, kUnmatchedVSGlyphId);

  hb_codepoint_t glyph_from_font_without_vs =
      GetGlyphForEmojiVSFromFontWithBaseCharOnly(character, 0);
  EXPECT_TRUE(glyph_from_font_without_vs);
  EXPECT_NE(glyph_from_font_without_vs, kUnmatchedVSGlyphId);
}
#endif

}  // namespace blink
