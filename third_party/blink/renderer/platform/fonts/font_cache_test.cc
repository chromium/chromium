// Copyright 2014 The Chromium Authors
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
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

class FontCacheTest : public FontTestBase {};

TEST_F(FontCacheTest, getLastResortFallbackFont) {
  FontCache& font_cache = FontCache::Get();

  // Perform the test for the default font family (kStandardFamily) and the
  // -webkit-body font family (kWebkitBodyFamily) since they behave the same in
  // term of font/glyph selection.
  // TODO(crbug.com/1065468): Remove the test for kWebkitBodyFamily when
  // -webkit-body in unshipped.
  for (FontDescription::GenericFamilyType family_type :
       {FontDescription::kStandardFamily, FontDescription::kWebkitBodyFamily,
        FontDescription::kSansSerifFamily}) {
    FontDescription font_description;
    font_description.SetGenericFamily(family_type);
    const SimpleFontData* font_data =
        font_cache.GetLastResortFallbackFont(font_description);
    EXPECT_TRUE(font_data);
  }
}

TEST_F(FontCacheTest, NoFallbackForPrivateUseArea) {
  FontCache& font_cache = FontCache::Get();

  // Perform the test for the default font family (kStandardFamily) and the
  // -webkit-body font family (kWebkitBodyFamily) since they behave the same in
  // term of font/glyph selection.
  // TODO(crbug.com/1065468): Remove the test for kWebkitBodyFamily when
  // -webkit-body in unshipped.
  for (FontDescription::GenericFamilyType family_type :
       {FontDescription::kStandardFamily, FontDescription::kWebkitBodyFamily}) {
    FontDescription font_description;
    font_description.SetGenericFamily(family_type);
    for (UChar32 character : {0xE000, 0xE401, 0xE402, 0xE403, 0xF8FF, 0xF0000,
                              0xFAAAA, 0x100000, 0x10AAAA}) {
      const SimpleFontData* font_data = font_cache.FallbackFontForCharacter(
          font_description, character, nullptr);
      EXPECT_EQ(font_data, nullptr);
    }
  }
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(FontCacheTest, FallbackForEmojis) {
  FontCache& font_cache = FontCache::Get();

  // Perform the test for the default font family (kStandardFamily) and the
  // -webkit-body font family (kWebkitBodyFamily) since they behave the same in
  // term of font/glyph selection.
  // TODO(crbug.com/1065468): Remove the test for kWebkitBodyFamily when
  // -webkit-body in unshipped.
  for (FontDescription::GenericFamilyType family_type :
       {FontDescription::kStandardFamily, FontDescription::kWebkitBodyFamily}) {
    FontDescription font_description;
    font_description.SetGenericFamily(family_type);

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
        const SimpleFontData* font_data = font_cache.FallbackFontForCharacter(
            font_description, character, nullptr,
            FontFallbackPriority::kEmojiEmoji);
        EXPECT_EQ(font_data->PlatformData().FontFamilyName(),
                  String::FromUtf8(kNotoColorEmoji))
            << "Character " << character_utf8
            << " doesn't match what we expected for kEmojiEmoji.";
      }
      {
        const SimpleFontData* font_data = font_cache.FallbackFontForCharacter(
            font_description, character, nullptr,
            FontFallbackPriority::kEmojiText);
        if (available_in_contour_font) {
          EXPECT_NE(font_data->PlatformData().FontFamilyName(),
                    String::FromUtf8(kNotoColorEmoji))
              << "Character " << character_utf8
              << " doesn't match what we expected for kEmojiText.";
        } else {
          EXPECT_EQ(font_data->PlatformData().FontFamilyName(),
                    String::FromUtf8(kNotoColorEmoji))
              << "Character " << character_utf8
              << " doesn't match what we expected for kEmojiText.";
        }
      }
    }
  }
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

TEST_F(FontCacheTest, firstAvailableOrFirst) {
  EXPECT_TRUE(FontCache::FirstAvailableOrFirst("").empty());
  EXPECT_TRUE(FontCache::FirstAvailableOrFirst(String()).empty());

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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
// local() font matching requires a Mojo connection which is not available in
// unit tests.
#define MAYBE_FontUniqueNameMatchAvailable DISABLED_FontUniqueNameMatchAvailable
#else
#define MAYBE_FontUniqueNameMatchAvailable FontUniqueNameMatchAvailable
#endif
TEST_F(FontCacheTest, MAYBE_FontUniqueNameMatchAvailable) {
  FontCache& font_cache = FontCache::Get();

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);
  font_description.SetComputedSize(12.f);
  FontFaceCreationParams creation_params;
  EXPECT_FALSE(font_cache.IsPlatformFontUniqueNameMatchAvailable(
      font_description, AtomicString()));
  EXPECT_TRUE(font_cache.IsPlatformFontUniqueNameMatchAvailable(
      font_description, AtomicString("Arial")));
  EXPECT_FALSE(font_cache.IsPlatformFontUniqueNameMatchAvailable(
      font_description, AtomicString("INVALID_FONT_NAME")));
}

// Unfortunately, we can't ensure a font here since on Android and Mac the
// unittests can't access the font configuration. However, this test passes
// when it's not crashing in FontCache.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_GetLargerThanMaxUnsignedFont DISABLED_GetLargerThanMaxUnsignedFont
#else
#define MAYBE_GetLargerThanMaxUnsignedFont GetLargerThanMaxUnsignedFont
#endif
// https://crbug.com/969402
TEST_F(FontCacheTest, MAYBE_GetLargerThanMaxUnsignedFont) {
  FontCache& font_cache = FontCache::Get();

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);
  font_description.SetComputedSize(
      static_cast<float>(std::numeric_limits<unsigned>::max()) + 1.f);
  FontFaceCreationParams creation_params;
  const blink::SimpleFontData* font_data =
      font_cache.GetFontData(font_description, AtomicString());
  EXPECT_TRUE(font_data);
}

#if !BUILDFLAG(IS_MAC)
TEST_F(FontCacheTest, systemFont) {
  FontCache::SystemFontFamily();
  // Test the function does not crash. Return value varies by system and config.
}
#endif

#if BUILDFLAG(IS_ANDROID)
TEST_F(FontCacheTest, Locale) {
  FontCacheKey key1(FontFaceCreationParams(), /* font_size */ 16,
                    /* options */ 0, /* device_scale_factor */ 1.0f,
                    /* size_adjust */ FontSizeAdjust(),
                    /* variation_settings */ nullptr,
                    /* palette */ nullptr,
                    /* variant_alternates */ nullptr,
                    /* is_unique_match */ false);
  FontCacheKey key2 = key1;
  EXPECT_EQ(key1.GetHash(), key2.GetHash());
  EXPECT_EQ(key1, key2);

  key2.SetLocale(AtomicString("ja"));
  EXPECT_NE(key1.GetHash(), key2.GetHash());
  EXPECT_NE(key1, key2);
}

TEST_F(FontCacheTest, PrewarmFamily) {
  test::ScopedTestFontPrewarmer prewarmer;
  EXPECT_EQ(prewarmer.PrewarmedFamilyNames().size(), 0u);
  FontCache::PrewarmFamily(AtomicString("test-font-cache-prewarm-family"));
  EXPECT_EQ(prewarmer.PrewarmedFamilyNames().size(), 1u);
  EXPECT_EQ(prewarmer.PrewarmedFamilyNames()[0],
            "test-font-cache-prewarm-family");
}
#endif  // BUILDFLAG(IS_ANDROID)

// Only these platforms resolve font unique names through
// FontCache::CreateTypefaceFromUniqueName(); macOS goes through CoreText,
// which shares the underlying font data itself.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)

namespace {

// A FontUniqueNameLookup that resolves every name to the same typeface and
// counts how often it was consulted.
class CountingFontUniqueNameLookup : public FontUniqueNameLookup {
 public:
  explicit CountingFontUniqueNameLookup(sk_sp<SkTypeface> typeface)
      : typeface_(std::move(typeface)) {}

  sk_sp<SkTypeface> MatchUniqueName(const String& font_unique_name) override {
    ++match_count_;
    return typeface_;
  }

  wtf_size_t MatchCount() const { return match_count_; }

 private:
  sk_sp<SkTypeface> typeface_;
  wtf_size_t match_count_ = 0;
};

// Installs a CountingFontUniqueNameLookup for the duration of the test and
// restores the platform default afterwards.
class ScopedCountingFontUniqueNameLookup {
  STACK_ALLOCATED();

 public:
  explicit ScopedCountingFontUniqueNameLookup(sk_sp<SkTypeface> typeface) {
    auto lookup =
        std::make_unique<CountingFontUniqueNameLookup>(std::move(typeface));
    lookup_ = lookup.get();
    FontGlobalContext::SetFontUniqueNameLookupForTesting(std::move(lookup));
  }

  ~ScopedCountingFontUniqueNameLookup() {
    FontGlobalContext::SetFontUniqueNameLookupForTesting(nullptr);
    FontCache::Get().Invalidate();
  }

  wtf_size_t MatchCount() const { return lookup_->MatchCount(); }

 private:
  CountingFontUniqueNameLookup* lookup_ = nullptr;
};

// Returns any typeface available on the system, to stand in for a local font.
sk_sp<SkTypeface> AnyTypeface() {
  const SimpleFontData* font_data =
      FontCache::Get().GetLastResortFallbackFont(FontDescription());
  return font_data ? font_data->PlatformData().TypefaceSp() : nullptr;
}

const SimpleFontData* GetUniqueNameFontData(const AtomicString& unique_name,
                                            float size) {
  FontDescription description;
  description.SetComputedSize(size);
  return FontCache::Get().GetFontData(description, unique_name,
                                      AlternateFontName::kLocalUniqueFace);
}

}  // namespace

// A local() font used at several sizes must resolve to one shared typeface,
// rather than mapping the underlying font file once per size.
// Regression test for crbug.com/325826179.
TEST_F(FontCacheTest, LocalUniqueFaceTypefaceSharedAcrossFontSizes) {
  ScopedFontUniqueNameTypefaceCacheForTest enable_cache(true);

  sk_sp<SkTypeface> typeface = AnyTypeface();
  ASSERT_TRUE(typeface);

  ScopedCountingFontUniqueNameLookup lookup(typeface);
  const AtomicString unique_name("TestFontCacheUniqueName");

  for (float size : {12.0f, 16.0f, 24.0f, 48.0f}) {
    const SimpleFontData* font_data = GetUniqueNameFontData(unique_name, size);
    ASSERT_TRUE(font_data) << "font size " << size;
    EXPECT_EQ(typeface.get(), font_data->PlatformData().Typeface())
        << "font size " << size;
  }

  // Each size gets its own FontPlatformData, but the unique name must have
  // been resolved to a typeface only once.
  EXPECT_EQ(1u, lookup.MatchCount());
}

// The typeface cache must not outlive a font cache invalidation, which is also
// how it is purged under memory pressure.
TEST_F(FontCacheTest, LocalUniqueFaceTypefaceCacheClearedOnInvalidate) {
  ScopedFontUniqueNameTypefaceCacheForTest enable_cache(true);

  sk_sp<SkTypeface> typeface = AnyTypeface();
  ASSERT_TRUE(typeface);

  ScopedCountingFontUniqueNameLookup lookup(typeface);
  const AtomicString unique_name("TestFontCacheUniqueName");

  ASSERT_TRUE(GetUniqueNameFontData(unique_name, 16.0f));
  EXPECT_EQ(1u, lookup.MatchCount());

  FontCache::Get().Invalidate();

  ASSERT_TRUE(GetUniqueNameFontData(unique_name, 16.0f));
  EXPECT_EQ(2u, lookup.MatchCount());
}

// With the feature disabled, the unique name is resolved once per font size,
// i.e. the behavior before the cache was introduced.
TEST_F(FontCacheTest, LocalUniqueFaceTypefaceNotCachedWhenDisabled) {
  ScopedFontUniqueNameTypefaceCacheForTest disable_cache(false);

  sk_sp<SkTypeface> typeface = AnyTypeface();
  ASSERT_TRUE(typeface);

  ScopedCountingFontUniqueNameLookup lookup(typeface);
  const AtomicString unique_name("TestFontCacheUniqueName");

  for (float size : {12.0f, 16.0f, 24.0f, 48.0f}) {
    ASSERT_TRUE(GetUniqueNameFontData(unique_name, size))
        << "font size " << size;
  }

  EXPECT_EQ(4u, lookup.MatchCount());
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

}  // namespace blink
