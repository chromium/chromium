// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_face_source.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

namespace blink {

class DummyFontFaceSource : public CSSFontFaceSource {
 public:
  scoped_refptr<SimpleFontData> CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) override {
    return SimpleFontData::Create(FontPlatformData(
        SkTypeface::MakeDefault(), std::string(), 0, false, false));
  }

  DummyFontFaceSource() = default;

  scoped_refptr<SimpleFontData> GetFontDataForSize(float size) {
    FontDescription font_description;
    font_description.SetSizeAdjust(size);
    font_description.SetAdjustedSize(size);
    FontSelectionCapabilities normal_capabilities(
        {NormalWidthValue(), NormalWidthValue()},
        {NormalSlopeValue(), NormalSlopeValue()},
        {NormalWeightValue(), NormalWeightValue()});
    return GetFontData(font_description, normal_capabilities);
  }
};

namespace {

unsigned SimulateHashCalculation(float size) {
  FontDescription font_description;
  font_description.SetSizeAdjust(size);
  font_description.SetAdjustedSize(size);
  bool is_unique_match = false;
  return font_description.CacheKey(FontFaceCreationParams(), is_unique_match)
      .GetHash();
}
}

TEST(CSSFontFaceSourceTest, HashCollision) {
  DummyFontFaceSource font_face_source;
  // Even if the hash value collide, fontface cache should return different
  // value for different fonts, values determined experimentally.
  EXPECT_EQ(SimulateHashCalculation(10280), SimulateHashCalculation(9875));
  EXPECT_NE(font_face_source.GetFontDataForSize(10280),
            font_face_source.GetFontDataForSize(9875));
}

// Exercises the size font_data_table_ assertions in CSSFontFaceSource.
TEST(CSSFontFaceSourceTest, UnboundedGrowth) {
  DummyFontFaceSource font_face_source;
  FontDescription font_description_variable;
  FontSelectionCapabilities normal_capabilities(
      {NormalWidthValue(), NormalWidthValue()},
      {NormalSlopeValue(), NormalSlopeValue()},
      {NormalWeightValue(), NormalWeightValue()});

  // Roughly 3000 font variants.
  for (float wght = 700; wght < 705; wght += 1 / 6.f) {
    for (float wdth = 100; wdth < 125; wdth += 1 / 4.f) {
      scoped_refptr<FontVariationSettings> variation_settings =
          FontVariationSettings::Create();
      variation_settings->Append(FontVariationAxis("wght", wght));
      variation_settings->Append(FontVariationAxis("wdth", wdth));
      font_description_variable.SetVariationSettings(variation_settings);
      font_face_source.GetFontData(font_description_variable,
                                   normal_capabilities);
    }
  }
}

}  // namespace blink
