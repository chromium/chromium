// Copyright 2014 The Chromium Authors. All rights reserved.
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
    FontCache* font_cache = FontCache::GetFontCache();
    FontDescription font_description;
    font_description.SetLocale(locale);
    font_description.SetGenericFamily(FontDescription::kSerifFamily);
    return font_cache->CreateLocaleSpecificTypeface(font_description, "serif");
  }

  FontCachePurgePreventer purge_preventer;
};

TEST_F(FontCacheAndroidTest, FallbackFontForCharacter) {
  // A Latin character in the common locale system font, but not in the
  // Chinese locale-preferred font.
  const UChar32 kTestChar = 228;

  FontDescription font_description;
  font_description.SetLocale(LayoutLocale::Get("zh"));
  ASSERT_EQ(USCRIPT_SIMPLIFIED_HAN, font_description.GetScript());
  font_description.SetGenericFamily(FontDescription::kStandardFamily);

  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);
  scoped_refptr<SimpleFontData> font_data =
      font_cache->FallbackFontForCharacter(font_description, kTestChar, 0);
  EXPECT_TRUE(font_data);
}

TEST_F(FontCacheAndroidTest, FallbackFontForCharacterSerif) {
  // Test is valid only if the system has a locale-specific `serif`.
  const LayoutLocale* ja = LayoutLocale::Get("ja");
  sk_sp<SkTypeface> serif_ja_typeface = CreateSerifTypeface(ja);
  if (!serif_ja_typeface)
    return;

  // When |GenericFamily| set to |kSerifFamily|, it should find the
  // locale-specific serif font.
  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kSerifFamily);
  font_description.SetLocale(ja);
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);
  const UChar32 kTestChar = 0x4E00;  // U+4E00 CJK UNIFIED IDEOGRAPH-4E00
  scoped_refptr<SimpleFontData> font_data =
      font_cache->FallbackFontForCharacter(font_description, kTestChar,
                                           nullptr);
  EXPECT_TRUE(font_data);
  EXPECT_EQ(serif_ja_typeface.get(), font_data->PlatformData().Typeface());
}

TEST_F(FontCacheAndroidTest, LocaleSpecificTypeface) {
  // Test is valid only if the system has a locale-specific `serif`.
  const LayoutLocale* ja = LayoutLocale::Get("ja");
  sk_sp<SkTypeface> serif_ja_typeface = CreateSerifTypeface(ja);
  if (!serif_ja_typeface)
    return;

  // If the system has one, it must be different from the default font.
  FontDescription standard_ja_description;
  standard_ja_description.SetLocale(ja);
  standard_ja_description.SetGenericFamily(FontDescription::kStandardFamily);
  std::string name;
  FontCache* font_cache = FontCache::GetFontCache();
  sk_sp<SkTypeface> standard_ja_typeface = font_cache->CreateTypeface(
      standard_ja_description, FontFaceCreationParams(), name);
  EXPECT_NE(serif_ja_typeface.get(), standard_ja_typeface.get());
}

TEST(FontCacheAndroid, GenericFamilyNameForScript) {
  FontDescription english;
  english.SetLocale(LayoutLocale::Get("en"));
  FontDescription chinese;
  chinese.SetLocale(LayoutLocale::Get("zh"));

  font_family_names::Init();
  // For non-CJK, getGenericFamilyNameForScript should return the given
  // familyName.
  EXPECT_EQ(font_family_names::kWebkitStandard,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kWebkitStandard, english));
  EXPECT_EQ(font_family_names::kWebkitMonospace,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kWebkitMonospace, english));

  // For CJK, getGenericFamilyNameForScript should return CJK fonts except
  // monospace.
  EXPECT_NE(font_family_names::kWebkitStandard,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kWebkitStandard, chinese));
  EXPECT_EQ(font_family_names::kWebkitMonospace,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kWebkitMonospace, chinese));
}

}  // namespace blink
