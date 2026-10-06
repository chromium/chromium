// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/mac/character_fallback_cache.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreText.h>

#include "base/apple/scoped_cftyperef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"

using base::apple::ScopedCFTypeRef;

namespace blink {

namespace {
constexpr float kTestFontSize = 16.0f;
}

TEST(CharacterFallbackKey, SystemFontJA) {
  ScopedCFTypeRef<CTFontRef> ja_ui_font(CTFontCreateUIFontForLanguage(
      kCTFontUIFontSystem, kTestFontSize, CFSTR("ja-JA")));
  EXPECT_TRUE(ja_ui_font);
  std::optional<CharacterFallbackKey> key = CharacterFallbackKey::Make(
      ja_ui_font.get(), kNormalWeightValue.RawValue(),
      kNormalSlopeValue.RawValue(),
      static_cast<uint8_t>(FontOrientation::kHorizontal), kTestFontSize);

  EXPECT_TRUE(key.has_value());
  EXPECT_EQ(key->font_identifier, "UIFONTUSAGE:CTFontRegularUsage-LANG:ja-JA");
  EXPECT_EQ(key->weight, 1600);
  EXPECT_EQ(key->style, 0);
  EXPECT_EQ(CharacterFallbackKeyHashTraits::GetHash(*key), 2935333743u);
}

TEST(CharacterFallbackKey, SystemFontZHHans) {
  ScopedCFTypeRef<CTFontRef> hans_ui_font(CTFontCreateUIFontForLanguage(
      kCTFontUIFontSystem, kTestFontSize, CFSTR("zh-Hans")));
  EXPECT_TRUE(hans_ui_font);
  std::optional<CharacterFallbackKey> key = CharacterFallbackKey::Make(
      hans_ui_font.get(), kNormalWeightValue.RawValue(),
      kNormalSlopeValue.RawValue(),
      static_cast<uint8_t>(FontOrientation::kHorizontal), kTestFontSize);

  EXPECT_TRUE(key.has_value());
  EXPECT_EQ(key->font_identifier,
            "UIFONTUSAGE:CTFontRegularUsage-LANG:zh-Hans");
  EXPECT_EQ(key->weight, 1600);
  EXPECT_EQ(key->style, 0);
  EXPECT_EQ(CharacterFallbackKeyHashTraits::GetHash(*key), 1948861267u);
}

TEST(CharacterFallbackKey, SystemFontZHHant) {
  ScopedCFTypeRef<CTFontRef> hant_ui_font(CTFontCreateUIFontForLanguage(
      kCTFontUIFontSystem, kTestFontSize, CFSTR("zh-Hant")));
  EXPECT_TRUE(hant_ui_font);
  std::optional<CharacterFallbackKey> key = CharacterFallbackKey::Make(
      hant_ui_font.get(), kNormalWeightValue.RawValue(),
      kNormalSlopeValue.RawValue(),
      static_cast<uint8_t>(FontOrientation::kHorizontal), kTestFontSize);

  EXPECT_TRUE(key.has_value());
  EXPECT_EQ(key->font_identifier,
            "UIFONTUSAGE:CTFontRegularUsage-LANG:zh-Hant");
  EXPECT_EQ(key->weight, 1600);
  EXPECT_EQ(key->style, 0);
  EXPECT_EQ(CharacterFallbackKeyHashTraits::GetHash(*key), 188546976u);
}

TEST(CharacterFallbackKey, EmojiFont) {
  ScopedCFTypeRef<CTFontRef> emoji_font(
      CTFontCreateWithName(CFSTR("Apple Color Emoji"), kTestFontSize, nullptr));
  EXPECT_TRUE(emoji_font);
  std::optional<CharacterFallbackKey> key = CharacterFallbackKey::Make(
      emoji_font.get(), kNormalWeightValue.RawValue(),
      kNormalSlopeValue.RawValue(),
      static_cast<uint8_t>(FontOrientation::kHorizontal), kTestFontSize);

  EXPECT_TRUE(key.has_value());
  EXPECT_EQ(key->font_identifier, "AppleColorEmoji");
  EXPECT_EQ(key->weight, 1600);
  EXPECT_EQ(key->style, 0);
  EXPECT_EQ(CharacterFallbackKeyHashTraits::GetHash(*key), 2157453050u);
}

TEST(CharacterFallbackKey, Helvetica) {
  ScopedCFTypeRef<CTFontRef> helvetica_font(
      CTFontCreateWithName(CFSTR("Helvetica"), kTestFontSize, nullptr));
  EXPECT_TRUE(helvetica_font);
  std::optional<CharacterFallbackKey> key = CharacterFallbackKey::Make(
      helvetica_font.get(), kNormalWeightValue.RawValue(),
      kNormalSlopeValue.RawValue(),
      static_cast<uint8_t>(FontOrientation::kHorizontal), kTestFontSize);

  EXPECT_TRUE(key.has_value());
  EXPECT_EQ(key->font_identifier, "Helvetica");
  EXPECT_EQ(key->weight, 1600);
  EXPECT_EQ(key->style, 0);
  EXPECT_EQ(CharacterFallbackKeyHashTraits::GetHash(*key), 1508599061u);
}

}  // namespace blink
