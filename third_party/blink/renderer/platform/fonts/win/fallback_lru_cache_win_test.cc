// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/win/fallback_family_style_cache_win.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace {

const char kHanSimplifiedLocale[] = "zh-Hans";
const size_t kLruCacheTestSize = 5;
const char kFontFamilyNameArial[] = "Arial";
const UChar32 kFirstCJKIdeograph = 0x4E00;
const UChar32 kSecondCJKIdeograph = kFirstCJKIdeograph + 1;

sk_sp<SkTypeface> fallbackForLocale(String locale, UChar32 codepoint) {
  sk_sp<SkFontMgr> font_mgr = SkFontMgr::RefDefault();
  std::string locale_string(locale.Ascii());
  const char* locale_char = locale_string.c_str();
  return sk_sp<SkTypeface>(font_mgr->matchFamilyStyleCharacter(
      kFontFamilyNameArial, SkFontStyle(), &locale_char, 1, codepoint));
}

void fillCacheWithDummies(blink::FallbackLruCache& lru_cache,
                          const char* format_string,
                          size_t count) {
  for (size_t i = 0; i < count; ++i) {
    blink::TypefaceVector dummy_typefaces;
    dummy_typefaces.push_back(
        SkTypeface::MakeFromName(kFontFamilyNameArial, SkFontStyle()));
    lru_cache.Put(String::Format(format_string, i), std::move(dummy_typefaces));
  }
}

}  // namespace

namespace blink {

TEST(FallbackLruCacheTest, KeepChineseWhenFetched) {
  // Put a Chinese font in the cache, add size - 1 more dummy fallback fonts so
  // that the cache is full. Get() and verify typeface for Chinese to move them
  // up to the top of the cache. Then fill again with size - 1 items and verify
  // that Chinese is still in the cache. Then fill with # size items to evict
  // the Chinese font and ensure it's gone.
  FallbackLruCache lru_cache(kLruCacheTestSize);
  EXPECT_EQ(lru_cache.size(), 0u);
  TypefaceVector fallback_typefaces_zh;
  fallback_typefaces_zh.push_back(
      fallbackForLocale(kHanSimplifiedLocale, kFirstCJKIdeograph));
  lru_cache.Put(kHanSimplifiedLocale, std::move(fallback_typefaces_zh));

  EXPECT_EQ(lru_cache.size(), 1u);

  fillCacheWithDummies(lru_cache, "dummy_locale_%zu", kLruCacheTestSize - 1);
  TypefaceVector* chinese_typefaces = lru_cache.Get(kHanSimplifiedLocale);
  EXPECT_TRUE(chinese_typefaces);
  EXPECT_TRUE(chinese_typefaces->at(0)->unicharToGlyph(0x4E01));
  EXPECT_EQ(lru_cache.size(), kLruCacheTestSize);

  fillCacheWithDummies(lru_cache, "dummy_locale_2nd_%zu",
                       kLruCacheTestSize - 1);
  chinese_typefaces = nullptr;
  chinese_typefaces = lru_cache.Get(kHanSimplifiedLocale);
  EXPECT_TRUE(chinese_typefaces);
  EXPECT_EQ(chinese_typefaces->size(), 1u);
  EXPECT_TRUE(chinese_typefaces->at(0)->unicharToGlyph(kSecondCJKIdeograph));
  EXPECT_EQ(lru_cache.size(), kLruCacheTestSize);

  fillCacheWithDummies(lru_cache, "dummy_locale_3rd_%zu", kLruCacheTestSize);
  chinese_typefaces = nullptr;
  chinese_typefaces = lru_cache.Get(kHanSimplifiedLocale);
  EXPECT_FALSE(chinese_typefaces);
  EXPECT_EQ(lru_cache.size(), kLruCacheTestSize);
}

TEST(FallbackLruCacheTest, LargeFillAndClear) {
  FallbackLruCache lru_cache(kLruCacheTestSize);
  EXPECT_EQ(lru_cache.size(), 0u);
  fillCacheWithDummies(lru_cache, "dummy_locale_%zu", 1000);
  EXPECT_EQ(lru_cache.size(), kLruCacheTestSize);
  lru_cache.Clear();
  EXPECT_EQ(lru_cache.size(), 0u);
}

TEST(FallbackLruCacheTest, KeyOverride) {
  FallbackLruCache lru_cache(kLruCacheTestSize);
  EXPECT_EQ(lru_cache.size(), 0u);
  fillCacheWithDummies(lru_cache, "same_locale", 10);
  EXPECT_EQ(lru_cache.size(), 1u);
}

}  // namespace blink
