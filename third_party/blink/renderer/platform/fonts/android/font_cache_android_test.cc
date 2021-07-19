// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

namespace blink {

TEST(FontCacheAndroid, FallbackFontForCharacter) {
  // A Latin character in the common locale system font, but not in the
  // Chinese locale-preferred font.
  const UChar32 kTestChar = 228;

  FontCachePurgePreventer purge_preventer;
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

TEST(FontCacheAndroid, LocaleSpecificTypeface) {
  FontCachePurgePreventer purge_preventer;
  FontCache* font_cache = FontCache::GetFontCache();

  FontDescription serif_ja_description;
  serif_ja_description.SetLocale(LayoutLocale::Get("ja"));
  serif_ja_description.SetGenericFamily(FontDescription::kSerifFamily);
  sk_sp<SkTypeface> serif_ja_typeface =
      font_cache->CreateLocaleSpecificTypeface(serif_ja_description, "serif");

  // |CreateLocaleSpecificTypeface| returns `nullptr` if the system does not
  // have a locale-specific `serif` for Japanese. In this case, we can't test
  // further.
  if (!serif_ja_typeface)
    return;

  // If the system has one, it must be different from the default font.
  FontDescription standard_ja_description;
  standard_ja_description.SetLocale(LayoutLocale::Get("ja"));
  standard_ja_description.SetGenericFamily(FontDescription::kStandardFamily);
  std::string name;
  sk_sp<SkTypeface> standard_ja_typeface = font_cache->CreateTypeface(
      standard_ja_description, FontFaceCreationParams(), name);
  EXPECT_NE(serif_ja_typeface.get(), standard_ja_typeface.get());
}

}  // namespace blink
