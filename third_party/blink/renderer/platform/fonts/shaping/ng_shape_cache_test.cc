// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/ng_shape_cache.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_performance.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

class NGShapeCacheTest : public FontTestBase {
 protected:
  void SetUp() override {
    font = test::CreateAhemFont(100);
    cache = MakeGarbageCollected<NGShapeCache>(font->PrimaryFont());
  }
  Persistent<Font> font;
  Persistent<NGShapeCache> cache;
};

TEST_F(NGShapeCacheTest, AddEntriesAndCacheHits) {
  auto shape_result_func = []() -> ShaperResult {
    // For the purposes of this test the actual internals of the shape result
    // doesn't matter.
    return {MakeGarbageCollected<ShapeResult>(0, 0, TextDirection::kLtr),
            /*can_cache=*/true};
  };

  auto create_key = [](const String& text,
                       TextDirection direction) -> ShapeCacheKey {
    return ShapeCacheKey(text, 0, text.length(), g_null_atom, {}, direction);
  };

  // Adding an entry is successful.
  const auto* entry_a_ltr = cache->GetOrCreate(
      create_key("A", TextDirection::kLtr),
      /*is_generic_family_or_last_resort=*/false, shape_result_func);
  ASSERT_TRUE(entry_a_ltr);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache->GetOrCreate(create_key("A", TextDirection::kLtr),
                               /*is_generic_family_or_last_resort=*/false,
                               shape_result_func),
            entry_a_ltr);

  // Adding the an entry with different text does not hit cache.
  const auto* entry_b_ltr = cache->GetOrCreate(
      create_key("B", TextDirection::kLtr),
      /*is_generic_family_or_last_resort=*/false, shape_result_func);
  ASSERT_TRUE(entry_b_ltr);
  EXPECT_NE(entry_b_ltr, entry_a_ltr);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache->GetOrCreate(create_key("B", TextDirection::kLtr),
                               /*is_generic_family_or_last_resort=*/false,
                               shape_result_func),
            entry_b_ltr);

  // Adding the an entry with different direction does not hit cache.
  const auto* entry_a_rtl = cache->GetOrCreate(
      create_key("A", TextDirection::kRtl),
      /*is_generic_family_or_last_resort=*/false, shape_result_func);
  ASSERT_TRUE(entry_a_rtl);
  EXPECT_NE(entry_a_rtl, entry_a_ltr);
  EXPECT_NE(entry_a_rtl, entry_b_ltr);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache->GetOrCreate(create_key("A", TextDirection::kRtl),
                               /*is_generic_family_or_last_resort=*/false,
                               shape_result_func),
            entry_a_rtl);
}

TEST_F(NGShapeCacheTest, FontPerformanceMetrics) {
  FontPerformance::Reset();

  auto ShapeResultFunc = []() -> ShaperResult {
    return {MakeGarbageCollected<ShapeResult>(0, 0, TextDirection::kLtr),
            /*can_cache=*/true};
  };

  auto CreateKey = [](const String& text) -> ShapeCacheKey {
    return ShapeCacheKey(text, 0, text.length(), g_null_atom, {},
                         TextDirection::kLtr);
  };

  // First access: Miss
  cache->GetOrCreate(CreateKey("A"),
                     /*is_generic_family_or_last_resort=*/false,
                     ShapeResultFunc);
  EXPECT_EQ(FontPerformance::ShapeCacheHitCount(), 0u);
  EXPECT_EQ(FontPerformance::ShapeCacheMissCount(), 1u);

  // Second access: Hit
  cache->GetOrCreate(CreateKey("A"),
                     /*is_generic_family_or_last_resort=*/false,
                     ShapeResultFunc);
  EXPECT_EQ(FontPerformance::ShapeCacheHitCount(), 1u);
  EXPECT_EQ(FontPerformance::ShapeCacheMissCount(), 1u);

  // Access with different key: Miss
  cache->GetOrCreate(CreateKey("B"),
                     /*is_generic_family_or_last_resort=*/false,
                     ShapeResultFunc);
  EXPECT_EQ(FontPerformance::ShapeCacheHitCount(), 1u);
  EXPECT_EQ(FontPerformance::ShapeCacheMissCount(), 2u);
}

TEST_F(NGShapeCacheTest, NotDefIsNotCached) {
  auto GetOrCreate = [&](const String& text) {
    const ShapeCacheKey key(text, 0, text.length(), g_null_atom, {},
                            TextDirection::kLtr);
    return cache->GetOrCreate(
        key, /*is_generic_family_or_last_resort=*/false, [&]() -> ShaperResult {
          HarfBuzzShaper shaper(text);
          return {shaper.Shape(font.Get(), TextDirection::kLtr),
                  /*can_cache=*/true};
        });
  };

  const String mapped_character("A");
  const ShapeResult* mapped_first = GetOrCreate(mapped_character);
  ASSERT_FALSE(mapped_first->HasNotDefGlyph());
  EXPECT_EQ(mapped_first, GetOrCreate(mapped_character));

  const String unmapped_character(u"\u0378");
  const ShapeResult* unmapped_first = GetOrCreate(unmapped_character);
  ASSERT_TRUE(unmapped_first->HasNotDefGlyph());
  EXPECT_NE(unmapped_first, GetOrCreate(unmapped_character));
}

TEST_F(NGShapeCacheTest, GenericOrLastResortPrivateUseBypassesCache) {
  const String private_use_character(u"\uE000");
  const ShapeCacheKey key(private_use_character, 0,
                          private_use_character.length(), g_null_atom, {},
                          TextDirection::kLtr);
  auto ShapeResultFunc = []() -> ShaperResult {
    return {MakeGarbageCollected<ShapeResult>(0, 0, TextDirection::kLtr),
            /*can_cache=*/true};
  };

  const ShapeResult* named_family_result = cache->GetOrCreate(
      key, /*is_generic_family_or_last_resort=*/false, ShapeResultFunc);
  EXPECT_EQ(named_family_result,
            cache->GetOrCreate(key,
                               /*is_generic_family_or_last_resort=*/false,
                               ShapeResultFunc));

  const ShapeResult* generic_family_result = cache->GetOrCreate(
      key, /*is_generic_family_or_last_resort=*/true, ShapeResultFunc);
  EXPECT_NE(named_family_result, generic_family_result);
  EXPECT_NE(generic_family_result,
            cache->GetOrCreate(key,
                               /*is_generic_family_or_last_resort=*/true,
                               ShapeResultFunc));
}

}  // namespace blink
