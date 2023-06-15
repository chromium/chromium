// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_palette.h"

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
}

TEST(FontPaletteTest, MixPaletteValue) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                       FontPalette::Create(FontPalette::kDarkPalette), 0.7, 1.0,
                       Color::ColorSpace::kSRGB, absl::nullopt);
  EXPECT_EQ("palette-mix(in srgb, light, dark 70%)", palette->ToString());
}

TEST(FontPaletteTest, NestedMixPaletteValue) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette_start = FontPalette::Create();
  scoped_refptr<FontPalette> palette_end =
      FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                       FontPalette::Create(FontPalette::kDarkPalette), 0.3, 1.0,
                       Color::ColorSpace::kSRGB, absl::nullopt);
  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 0.7, 1.0,
                       Color::ColorSpace::kOklab, absl::nullopt);
  EXPECT_EQ(
      "palette-mix(in oklab, normal, palette-mix(in srgb, light, dark 30%) "
      "70%)",
      palette->ToString());
}

TEST(FontPaletteTest, InterpolablePalettesNotEqual) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 0.1,
                       1.0, Color::ColorSpace::kOklab, absl::nullopt);
  scoped_refptr<FontPalette> palette2 = FontPalette::Mix(
      FontPalette::Create(FontPalette::kDarkPalette), FontPalette::Create(),
      0.1, 1.0, Color::ColorSpace::kOklab, absl::nullopt);
  EXPECT_FALSE(*palette1.get() == *palette2.get());
}

TEST(FontPaletteTest, InterpolableAndNonInterpolablePalettesNotEqual) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Create(FontPalette::kDarkPalette);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 0.1,
                       1.0, Color::ColorSpace::kSRGB, absl::nullopt);
  EXPECT_FALSE(*palette1.get() == *palette2.get());
}

TEST(FontPaletteTest, NonInterpolablePalettesNotEqual) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Create(FontPalette::kDarkPalette);
  palette1->SetMatchFamilyName("family1");
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Create(FontPalette::kDarkPalette);
  palette1->SetMatchFamilyName("family2");
  EXPECT_FALSE(*palette1.get() == *palette2.get());
}

TEST(FontPaletteTest, PalettesEqual) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 0.1,
                       1.0, Color::ColorSpace::kOklab, absl::nullopt);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 0.1,
                       1.0, Color::ColorSpace::kOklab, absl::nullopt);
  EXPECT_TRUE(*palette1.get() == *palette2.get());
}

}  // namespace blink
