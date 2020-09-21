// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include <unicode/unistr.h>
#include <string>
#include <tuple>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

TEST(FontCache, getLastResortFallbackFont) {
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);
  scoped_refptr<SimpleFontData> font_data =
      font_cache->GetLastResortFallbackFont(font_description, kRetain);
  EXPECT_TRUE(font_data);

  font_description.SetGenericFamily(FontDescription::kSansSerifFamily);
  font_data = font_cache->GetLastResortFallbackFont(font_description, kRetain);
  EXPECT_TRUE(font_data);
}

TEST(FontCache, NoFallbackForPrivateUseArea) {
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);
  for (UChar32 character : {0xE000, 0xE401, 0xE402, 0xE403, 0xF8FF, 0xF0000,
                            0xFAAAA, 0x100000, 0x10AAAA}) {
    scoped_refptr<SimpleFontData> font_data =
        font_cache->FallbackFontForCharacter(font_description, character,
                                             nullptr);
    EXPECT_EQ(font_data.get(), nullptr);
  }
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
TEST(FontCache, FallbackForEmojis) {
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);
  FontCachePurgePreventer purge_preventer;

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);

  static constexpr char kNotoColorEmoji[] = "Noto Color Emoji";

  // We should use structured binding when it becomes available...
  for (auto info : {
           std::pair<UChar32, bool>{U'☺', true},
           {U'👪', true},
           {U'🤣', false},
       }) {
    UChar32 character = info.first;
    // Set to true if the installed contour fonts support this glyph.
    bool available_in_contour_font = info.second;
    std::string character_utf8;
    icu::UnicodeString(character).toUTF8String(character_utf8);

    {
      scoped_refptr<SimpleFontData> font_data =
          font_cache->FallbackFontForCharacter(
              font_description, character, nullptr,
              FontFallbackPriority::kEmojiEmoji);
      EXPECT_EQ(font_data->PlatformData().FontFamilyName(), kNotoColorEmoji)
          << "Character " << character_utf8
          << " doesn't match what we expected for kEmojiEmoji.";
    }
    {
      scoped_refptr<SimpleFontData> font_data =
          font_cache->FallbackFontForCharacter(
              font_description, character, nullptr,
              FontFallbackPriority::kEmojiText);
      if (available_in_contour_font) {
        EXPECT_NE(font_data->PlatformData().FontFamilyName(), kNotoColorEmoji)
            << "Character " << character_utf8
            << " doesn't match what we expected for kEmojiText.";
      } else {
        EXPECT_EQ(font_data->PlatformData().FontFamilyName(), kNotoColorEmoji)
            << "Character " << character_utf8
            << " doesn't match what we expected for kEmojiText.";
      }
    }
  }
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

TEST(FontCache, firstAvailableOrFirst) {
  EXPECT_TRUE(FontCache::FirstAvailableOrFirst("").IsEmpty());
  EXPECT_TRUE(FontCache::FirstAvailableOrFirst(String()).IsEmpty());

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("Arial"));
  EXPECT_EQ("not exist", FontCache::FirstAvailableOrFirst("not exist"));

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("Arial, not exist"));
  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("not exist, Arial"));
  EXPECT_EQ("Arial",
            FontCache::FirstAvailableOrFirst("not exist, Arial, not exist"));

  EXPECT_EQ("not exist",
            FontCache::FirstAvailableOrFirst("not exist, not exist 2"));

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst(", not exist, Arial"));
  EXPECT_EQ("not exist",
            FontCache::FirstAvailableOrFirst(", not exist, not exist"));
}

// https://crbug.com/969402
TEST(FontCache, getLargerThanMaxUnsignedFont) {
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);
  font_description.SetComputedSize(std::numeric_limits<unsigned>::max() + 1.f);
  FontFaceCreationParams creation_params;
  scoped_refptr<blink::SimpleFontData> font_data =
      font_cache->GetFontData(font_description, AtomicString());
#if !defined(OS_ANDROID) && !defined(OS_MAC) && !defined(OS_WIN)
  // Unfortunately, we can't ensure a font here since on Android and Mac the
  // unittests can't access the font configuration. However, this test passes
  // when it's not crashing in FontCache.
  EXPECT_TRUE(font_data);
#endif
}

#if !defined(OS_MAC)
TEST(FontCache, systemFont) {
  FontCache::SystemFontFamily();
  // Test the function does not crash. Return value varies by system and config.
}
#endif

}  // namespace blink
