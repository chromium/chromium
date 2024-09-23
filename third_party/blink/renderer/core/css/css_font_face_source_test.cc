// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_face_source.h"

#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class DummyFontFaceSource : public CSSFontFaceSource {
 public:
  const SimpleFontData* CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) override {
    return MakeGarbageCollected<SimpleFontData>(
        MakeGarbageCollected<FontPlatformData>(
            skia::DefaultTypeface(), /* name */ std::string(),
            /* text_size */ 0, /* synthetic_bold */ false,
            /* synthetic_italic */ false, TextRenderingMode::kAutoTextRendering,
            ResolvedFontFeatures{}));
  }

  DummyFontFaceSource() = default;

  const SimpleFontData* GetFontDataForSize(float size) {
    FontDescription font_description;
    font_description.SetComputedSize(size);
    FontSelectionCapabilities normal_capabilities(
        {kNormalWidthValue, kNormalWidthValue},
        {kNormalSlopeValue, kNormalSlopeValue},
        {kNormalWeightValue, kNormalWeightValue});
    return GetFontData(font_description, normal_capabilities);
  }
};

namespace {

unsigned SimulateHashCalculation(float size) {
  FontDescription font_description;
  font_description.SetComputedSize(size);
  bool is_unique_match = false;
  return font_description.CacheKey(FontFaceCreationParams(), is_unique_match)
      .GetHash();
}
}  // namespace

TEST(CSSFontFaceSourceTest, HashCollision) {
  DummyFontFaceSource* font_face_source =
      MakeGarbageCollected<DummyFontFaceSource>();

  // Even if the hash values collide, fontface cache should return different
  // value for different fonts, values determined experimentally.
  constexpr float kEqualHashesFirst = 46317;
  constexpr float kEqualHashesSecond = 67002;
  EXPECT_EQ(SimulateHashCalculation(kEqualHashesFirst),
            SimulateHashCalculation(kEqualHashesSecond));
  EXPECT_NE(font_face_source->GetFontDataForSize(kEqualHashesFirst),
            font_face_source->GetFontDataForSize(kEqualHashesSecond));
}

// Exercises the size font_data_table_ assertions in CSSFontFaceSource.
TEST(CSSFontFaceSourceTest, UnboundedGrowth) {
  DummyFontFaceSource* font_face_source =
      MakeGarbageCollected<DummyFontFaceSource>();
  FontDescription font_description_variable;
  FontSelectionCapabilities normal_capabilities(
      {kNormalWidthValue, kNormalWidthValue},
      {kNormalSlopeValue, kNormalSlopeValue},
      {kNormalWeightValue, kNormalWeightValue});

  // Roughly 3000 font variants.
  for (float wght = 700; wght < 705; wght += 1 / 6.f) {
    for (float wdth = 100; wdth < 125; wdth += 1 / 4.f) {
      scoped_refptr<FontVariationSettings> variation_settings =
          FontVariationSettings::Create();
      variation_settings->Append(FontVariationAxis(AtomicString("wght"), wght));
      variation_settings->Append(FontVariationAxis(AtomicString("wdth"), wdth));
      font_description_variable.SetVariationSettings(variation_settings);
      font_face_source->GetFontData(font_description_variable,
                                    normal_capabilities);
    }
  }
}

}  // namespace blink
