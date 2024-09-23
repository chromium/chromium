// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

namespace blink {

class FontCacheAndroidTest : public testing::Test {
 protected:
  // Returns a locale-specific `serif` typeface, or `nullptr` if the system
  // does not have a locale-specific `serif`.
  sk_sp<SkTypeface> CreateSerifTypeface(const LayoutLocale* locale) {
    FontCache& font_cache = FontCache::Get();
    FontDescription font_description;
    font_description.SetLocale(locale);
    font_description.SetGenericFamily(FontDescription::kSerifFamily);
    return font_cache.CreateLocaleSpecificTypeface(font_description, "serif");
  }

  FontCachePurgePreventer purge_preventer;
};

TEST_F(FontCacheAndroidTest, FallbackFontForCharacter) {
  // Perform the test for the default font family (kStandardFamily) and the
  // -webkit-body font family (kWebkitBodyFamily) since they behave the same in
  // term of font/glyph selection.
  // TODO(crbug.com/1065468): Remove the test for kWebkitBodyFamily when
  // -webkit-body in unshipped.
  for (FontDescription::GenericFamilyType family_type :
       {FontDescription::kStandardFamily, FontDescription::kWebkitBodyFamily}) {
    // A Latin character in the common locale system font, but not in the
    // Chinese locale-preferred font.
    const UChar32 kTestChar = 228;

    FontDescription font_description;
    font_description.SetLocale(LayoutLocale::Get(AtomicString("zh")));
    ASSERT_EQ(USCRIPT_SIMPLIFIED_HAN, font_description.GetScript());
    font_description.SetGenericFamily(family_type);

    FontCache& font_cache = FontCache::Get();
    const SimpleFontData* font_data =
        font_cache.FallbackFontForCharacter(font_description, kTestChar, 0);
    EXPECT_TRUE(font_data);
  }
}

TEST_F(FontCacheAndroidTest, FallbackFontForCharacterSerif) {
  // Test is valid only if the system has a locale-specific `serif`.
  const LayoutLocale* ja = LayoutLocale::Get(AtomicString("ja"));
  sk_sp<SkTypeface> serif_ja_typeface = CreateSerifTypeface(ja);
  if (!serif_ja_typeface)
    return;

  // When |GenericFamily| set to |kSerifFamily|, it should find the
  // locale-specific serif font.
  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kSerifFamily);
  font_description.SetLocale(ja);
  FontCache& font_cache = FontCache::Get();
  const UChar32 kTestChar = 0x4E00;  // U+4E00 CJK UNIFIED IDEOGRAPH-4E00
  const SimpleFontData* font_data =
      font_cache.FallbackFontForCharacter(font_description, kTestChar, nullptr);
  EXPECT_TRUE(font_data);
  EXPECT_EQ(serif_ja_typeface.get(), font_data->PlatformData().Typeface());
}

TEST_F(FontCacheAndroidTest, LocaleSpecificTypeface) {
  // Perform the test for the default font family (kStandardFamily) and the
  // -webkit-body font family (kWebkitBodyFamily) since they behave the same in
  // term of font/glyph selection.
  // TODO(crbug.com/1065468): Remove the test for kWebkitBodyFamily when
  // -webkit-body in unshipped.
  for (FontDescription::GenericFamilyType family_type :
       {FontDescription::kStandardFamily, FontDescription::kWebkitBodyFamily}) {
    // Test is valid only if the system has a locale-specific `serif`.
    const LayoutLocale* ja = LayoutLocale::Get(AtomicString("ja"));
    sk_sp<SkTypeface> serif_ja_typeface = CreateSerifTypeface(ja);
    if (!serif_ja_typeface)
      return;

    // If the system has one, it must be different from the default font.
    FontDescription standard_ja_description;
    standard_ja_description.SetLocale(ja);
    standard_ja_description.SetGenericFamily(family_type);
    std::string name;
    FontCache& font_cache = FontCache::Get();
    sk_sp<SkTypeface> standard_ja_typeface = font_cache.CreateTypeface(
        standard_ja_description, FontFaceCreationParams(), name);
    EXPECT_NE(serif_ja_typeface.get(), standard_ja_typeface.get());
  }
}

// Check non-CJK locales do not create locale-specific typeface.
// TODO(crbug.com/1233315 crbug.com/1237860): Locale-specific serif is supported
// only for CJK until these issues were fixed.
TEST_F(FontCacheAndroidTest, LocaleSpecificTypefaceOnlyForCJK) {
  EXPECT_EQ(CreateSerifTypeface(LayoutLocale::Get(AtomicString("en"))),
            nullptr);
  // We can't test CJK locales return non-nullptr because not all devices on all
  // versions of Android have CJK serif fonts.
}

TEST(FontCacheAndroid, GenericFamilyNameForScript) {
  FontDescription english;
  english.SetLocale(LayoutLocale::Get(AtomicString("en")));
  FontDescription chinese;
  chinese.SetLocale(LayoutLocale::Get(AtomicString("zh")));

  AtomicString fallback("MyGenericFamilyNameFallback");

  font_family_names::Init();
  // For non-CJK, getGenericFamilyNameForScript should return the given
  // generic_family_name_fallback except monospace.
  EXPECT_EQ(fallback,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kWebkitStandard, fallback, english));
  EXPECT_EQ(font_family_names::kMonospace,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kMonospace, fallback, english));

  // For CJK, getGenericFamilyNameForScript should return CJK fonts except
  // monospace.
  EXPECT_NE(fallback,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kWebkitStandard, fallback, chinese));
  EXPECT_EQ(font_family_names::kMonospace,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kMonospace, fallback, chinese));
}

}  // namespace blink
