// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

namespace blink {

class FontCacheAndroidTest : public testing::Test {
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
  EXPECT_EQ(font_family_names::kMonospace,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kMonospace, english));

  // For CJK, getGenericFamilyNameForScript should return CJK fonts except
  // monospace.
  EXPECT_NE(font_family_names::kWebkitStandard,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kWebkitStandard, chinese));
  EXPECT_EQ(font_family_names::kMonospace,
            FontCache::GetGenericFamilyNameForScript(
                font_family_names::kMonospace, chinese));
}

}  // namespace blink
