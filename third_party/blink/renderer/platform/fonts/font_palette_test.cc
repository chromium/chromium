// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_palette.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

TEST(FontPaletteTest, HashingAndComparison) {
  scoped_refptr<FontPalette> a = FontPalette::Create();

  scoped_refptr<FontPalette> b =
      FontPalette::Create(FontPalette::kLightPalette);
  EXPECT_NE(a->GetHash(), b->GetHash());
  EXPECT_NE(a, b);

  b = FontPalette::Create(FontPalette::kDarkPalette);
  EXPECT_NE(a->GetHash(), b->GetHash());
  EXPECT_NE(a, b);

  b = FontPalette::Create(AtomicString("SomePaletteReference"));
  EXPECT_NE(a->GetHash(), b->GetHash());
  EXPECT_NE(a, b);

  b = FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                       FontPalette::Create(FontPalette::kDarkPalette), 30, 70,
                       0.7, 1.0, Color::ColorSpace::kSRGB, std::nullopt);
  EXPECT_NE(a->GetHash(), b->GetHash());
  EXPECT_NE(a, b);

  scoped_refptr<FontPalette> c =
      FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                       FontPalette::Create(FontPalette::kDarkPalette), 15, 35,
                       0.7, 1.0, Color::ColorSpace::kSRGB, std::nullopt);
  EXPECT_NE(c->GetHash(), b->GetHash());
  EXPECT_NE(c, b);

  c = FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                       FontPalette::Create(), 30, 70, 0.7, 1.0,
                       Color::ColorSpace::kSRGB, std::nullopt);
  EXPECT_NE(c->GetHash(), b->GetHash());
  EXPECT_NE(c, b);

  c = FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                       FontPalette::Create(FontPalette::kDarkPalette), 30, 70,
                       0.7, 1.0, Color::ColorSpace::kOklab, std::nullopt);
  EXPECT_NE(c->GetHash(), b->GetHash());
  EXPECT_NE(c, b);
}

TEST(FontPaletteTest, MixPaletteValue) {
  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                       FontPalette::Create(FontPalette::kDarkPalette), 30, 70,
                       0.7, 1.0, Color::ColorSpace::kSRGB, std::nullopt);
  EXPECT_EQ("palette-mix(in srgb, light, dark 70%)", palette->ToString());
}

TEST(FontPaletteTest, NestedMixPaletteValue) {
  scoped_refptr<FontPalette> palette_start = FontPalette::Create();
  scoped_refptr<FontPalette> palette_end =
      FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                       FontPalette::Create(FontPalette::kDarkPalette), 70, 30,
                       0.3, 1.0, Color::ColorSpace::kSRGB, std::nullopt);
  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 30, 70, 0.7, 1.0,
                       Color::ColorSpace::kOklab, std::nullopt);
  EXPECT_EQ(
      "palette-mix(in oklab, normal, palette-mix(in srgb, light, dark 30%) "
      "70%)",
      palette->ToString());
}

TEST(FontPaletteTest, InterpolablePalettesNotEqual) {
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 90, 10,
                       0.1, 1.0, Color::ColorSpace::kOklab, std::nullopt);
  scoped_refptr<FontPalette> palette2 = FontPalette::Mix(
      FontPalette::Create(FontPalette::kDarkPalette), FontPalette::Create(), 90,
      10, 0.1, 1.0, Color::ColorSpace::kOklab, std::nullopt);
  EXPECT_FALSE(*palette1.get() == *palette2.get());
}

TEST(FontPaletteTest, InterpolableAndNonInterpolablePalettesNotEqual) {
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Create(FontPalette::kDarkPalette);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 90, 10,
                       0.1, 1.0, Color::ColorSpace::kSRGB, std::nullopt);
  EXPECT_FALSE(*palette1.get() == *palette2.get());
}

TEST(FontPaletteTest, NonInterpolablePalettesNotEqual) {
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Create(FontPalette::kDarkPalette);
  palette1->SetMatchFamilyName(AtomicString("family1"));
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Create(FontPalette::kDarkPalette);
  palette1->SetMatchFamilyName(AtomicString("family2"));
  EXPECT_FALSE(*palette1.get() == *palette2.get());
}

TEST(FontPaletteTest, PalettesEqual) {
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 90, 10,
                       0.1, 1.0, Color::ColorSpace::kOklab, std::nullopt);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 90, 10,
                       0.1, 1.0, Color::ColorSpace::kOklab, std::nullopt);
  EXPECT_TRUE(*palette1.get() == *palette2.get());
}

TEST(FontPaletteTest, ComputeEndpointPercentagesFromNormalized) {
  FontPalette::NonNormalizedPercentages expected_percentages_1({50, 50});
  FontPalette::NonNormalizedPercentages actual_percentages_1 =
      FontPalette::ComputeEndpointPercentagesFromNormalized(0.5);

  FontPalette::NonNormalizedPercentages expected_percentages_2({70, 30});
  FontPalette::NonNormalizedPercentages actual_percentages_2 =
      FontPalette::ComputeEndpointPercentagesFromNormalized(0.3);

  FontPalette::NonNormalizedPercentages expected_percentages_3({0, 100});
  FontPalette::NonNormalizedPercentages actual_percentages_3 =
      FontPalette::ComputeEndpointPercentagesFromNormalized(1.0);

  EXPECT_EQ(expected_percentages_1, actual_percentages_1);
  EXPECT_EQ(expected_percentages_2, actual_percentages_2);
  EXPECT_EQ(expected_percentages_3, actual_percentages_3);
}

}  // namespace blink
