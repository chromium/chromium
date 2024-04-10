// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/glyph.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

hb_codepoint_t GetGlyphForVariationSequenceFromFont(
    Font font,
    UChar32 character,
    UChar32 variation_selector) {
  const FontPlatformData font_without_char_platform_data =
      font.PrimaryFont()->PlatformData();
  HarfBuzzFace* face_without_char =
      font_without_char_platform_data.GetHarfBuzzFace();
  EXPECT_TRUE(face_without_char);
  return face_without_char->HarfBuzzGetGlyphForTesting(character,
                                                       variation_selector);
}

void TestHarfBuzzGetNominalGlyphOnFontWithVS(bool ignore_variation_selectors) {
  ScopedFontVariationSequencesForTest scoped_feature(true);
  HarfBuzzFace::SetIgnoreVariationSelectors(ignore_variation_selectors);

  UChar32 character = 0xff01;
  UChar32 variation_selector = 0xfe01;

  Font font = test::CreateTestFont(
      AtomicString("Noto Sans CJK JP"),
      blink::test::BlinkWebTestsFontsTestDataPath(
          "noto/cjk/NotoSansCJKjp-Regular-subset-chws.otf"),
      11);
  hb_codepoint_t glyph =
      GetGlyphForVariationSequenceFromFont(font, character, variation_selector);
  EXPECT_TRUE(glyph);
  EXPECT_NE(glyph, kUnmatchedVSGlyphId);
}

hb_codepoint_t GetGlyphForVSFromFontWithBaseChar(
    bool ignore_variation_selectors) {
  ScopedFontVariationSequencesForTest scoped_feature(true);
  HarfBuzzFace::SetIgnoreVariationSelectors(ignore_variation_selectors);

  UChar32 character = 0x1820;
  UChar32 variation_selector = 0x180C;

  Font font = test::CreateTestFont(AtomicString("Noto Sans Mongolian"),
                                   blink::test::BlinkWebTestsFontsTestDataPath(
                                       "noto/NotoSansMongolian-regular.woff2"),
                                   11);
  return GetGlyphForVariationSequenceFromFont(font, character,
                                              variation_selector);
}

}  // namespace

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestFontWithVS) {
  TestHarfBuzzGetNominalGlyphOnFontWithVS(false);
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestFontWithVS_IgnoreVS) {
  TestHarfBuzzGetNominalGlyphOnFontWithVS(true);
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestFontWithBaseCharOnly) {
  EXPECT_EQ(GetGlyphForVSFromFontWithBaseChar(false), kUnmatchedVSGlyphId);
}

TEST(HarfBuzzFaceTest,
     HarfBuzzGetNominalGlyph_TestFontWithBaseCharOnly_IgnoreVS) {
  hb_codepoint_t glyph = GetGlyphForVSFromFontWithBaseChar(true);
  EXPECT_FALSE(glyph);
}

TEST(HarfBuzzFaceTest, HarfBuzzGetNominalGlyph_TestFontWithoutBaseChar) {
  ScopedFontVariationSequencesForTest scoped_feature(true);
  HarfBuzzFace::SetIgnoreVariationSelectors(false);

  UChar32 character = 0xff01;
  UChar32 variation_selector = 0xfe01;

  Font font = test::CreateAhemFont(11);
  EXPECT_FALSE(GetGlyphForVariationSequenceFromFont(font, character,
                                                    variation_selector));
}

}  // namespace blink
