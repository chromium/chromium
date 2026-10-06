// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"

namespace blink {

const FontSelectionValue Weights[] = {
    kThinWeightValue,   kExtraLightWeightValue, kLightWeightValue,
    kNormalWeightValue, kMediumWeightValue,     kSemiBoldWeightValue,
    kBoldWeightValue,   kExtraBoldWeightValue,  kBlackWeightValue};

class FontCacheMacTest
    : public FontTestBase,
      public testing::WithParamInterface<FontSelectionValue> {
 protected:
  FontDescription CreateFontDescriptionWithFontSynthesisNone(
      FontSelectionValue weight,
      FontSelectionValue style,
      FontSelectionValue stretch) {
    FontDescription font_description;

    font_description.SetWeight(weight);
    font_description.SetStyle(style);
    font_description.SetStretch(stretch);

    font_description.SetSyntheticBold(false);
    font_description.SetSyntheticItalic(false);

    font_description.SetFontSynthesisWeight(
        FontDescription::kAutoFontSynthesisWeight);
    font_description.SetFontSynthesisStyle(
        FontDescription::kAutoFontSynthesisStyle);

    return font_description;
  }

  void TestSystemUISyntheticBold() {
    FontSelectionValue weight = GetParam();

    FontCache& font_cache = FontCache::Get();
    for (size_t size = 5; size <= 30; size++) {
      FontDescription font_description =
          CreateFontDescriptionWithFontSynthesisNone(weight, kNormalSlopeValue,
                                                     kNormalWidthValue);
      const FontPlatformData* font_platform_data =
          font_cache.CreateFontPlatformData(
              font_description,
              FontFaceCreationParams(font_family_names::kSystemUi), size);
      EXPECT_FALSE(font_platform_data->SyntheticBold());
    }
  }

  size_t PlatformDataCacheSize() {
    return FontCache::Get().font_platform_data_cache_.map_.size();
  }
};

INSTANTIATE_TEST_SUITE_P(SystemUISyntheticBold,
                         FontCacheMacTest,
                         testing::ValuesIn(Weights));

TEST_P(FontCacheMacTest, SystemUISyntheticBoldCoreText) {
  TestSystemUISyntheticBold();
}

TEST_F(FontCacheMacTest, SystemUIEmojiTextPresentationUsesAppleSymbols) {
  FontCache& font_cache = FontCache::Get();
  FontDescription font_description;

  const SimpleFontData* system_ui_font =
      font_cache.GetFontData(font_description, font_family_names::kSystemUi);
  ASSERT_TRUE(system_ui_font);

  const SimpleFontData* fallback_font = font_cache.FallbackFontForCharacter(
      font_description, U'\U0001F310', system_ui_font,
      FontFallbackPriority::kEmojiTextWithVS);
  ASSERT_TRUE(fallback_font);
  EXPECT_EQ(fallback_font->PlatformData().FontFamilyName(),
            String::FromUtf8("Apple Symbols"));
}

TEST_F(FontCacheMacTest, InvalidateOnRegisteredFontsChanged) {
  FontCache& font_cache = FontCache::Get();
  FontDescription font_description;
  font_description.SetFamily(
      FontFamily(AtomicString("Arial"), FontFamily::Type::kFamilyName));

  // Populate the cache.
  font_cache.GetFontData(font_description, AtomicString("Arial"));
  EXPECT_GT(PlatformDataCacheSize(), 0u);

  // Trigger invalidation.
  RegisteredFontsChanged();

  // Verify the cache is cleared.
  EXPECT_EQ(PlatformDataCacheSize(), 0u);
}

TEST_F(FontCacheMacTest, UnavailableFontCaching) {
  FontCache& font_cache = FontCache::Get();
  AtomicString non_existent_family("NonExistentFontFamilyXYZ");

  // The font should not initially be marked as unavailable.
  EXPECT_FALSE(font_cache.IsFontFamilyUnavailable(non_existent_family));

  // A failed match attempt should mark the font as unavailable.
  MatchFontFamily(non_existent_family, kNormalWeightValue, kNormalSlopeValue,
                  kNormalWidthValue, 12.0f);
  EXPECT_TRUE(font_cache.IsFontFamilyUnavailable(non_existent_family));

  // The font should no longer be marked as unavailable after invalidation.
  font_cache.Invalidate();
  EXPECT_FALSE(font_cache.IsFontFamilyUnavailable(non_existent_family));
}

}  // namespace blink
