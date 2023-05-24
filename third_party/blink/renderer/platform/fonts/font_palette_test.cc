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

TEST(FontPaletteTest, NestedMixPaletteValue) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette_start = FontPalette::Create();
  scoped_refptr<FontPalette> palette_end =
      FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                       FontPalette::Create(FontPalette::kDarkPalette), 0.3);
  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 0.7);
  EXPECT_EQ("palette-mix(normal, palette-mix(light, dark, 0.3), 0.7)",
            palette->ToString());
}

TEST(FontPaletteTest, NestedMixAndScaledValue) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette_start =
      FontPalette::Scale(FontPalette::Create(FontPalette::kDarkPalette), 0.3);
  scoped_refptr<FontPalette> palette_end =
      FontPalette::Mix(FontPalette::Scale(FontPalette::Create(), 0.6),
                       FontPalette::Create(FontPalette::kLightPalette), 0.3);
  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 0.7);
  EXPECT_EQ(
      "palette-mix(palette-scale(dark, 0.3), palette-mix(palette-scale(normal, "
      "0.6), light, 0.3), 0.7)",
      palette->ToString());
}

TEST(FontPaletteTest, NestedAddAndScaleValue) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Scale(FontPalette::Create(FontPalette::kLightPalette), 0.3);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 0.1);
  scoped_refptr<FontPalette> palette = FontPalette::Add(palette1, palette2);
  EXPECT_EQ(
      "palette-add(palette-scale(light, 0.3), palette-mix(dark, light, 0.1))",
      palette->ToString());
}

TEST(FontPaletteTest, InterpolablePalettesWithSameOperationsNotEqual) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette1 = FontPalette::Scale(
      FontPalette::Add(
          FontPalette::Create(FontPalette::kLightPalette),
          FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                           FontPalette::Create(), 0.3)),
      0.1);
  scoped_refptr<FontPalette> palette2 = FontPalette::Scale(
      FontPalette::Add(
          FontPalette::Create(FontPalette::kLightPalette),
          FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                           FontPalette::Create(), 0.3)),
      0.1);
  EXPECT_FALSE(*palette1.get() == *palette2.get());
}

TEST(FontPaletteTest, InterpolablePalettesWithDifferentOperationsNotEqual) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Scale(FontPalette::Create(FontPalette::kLightPalette), 0.1);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(), 0.3);
  EXPECT_FALSE(*palette1.get() == *palette2.get());
}

TEST(FontPaletteTest, InterpolableAndNonInterpolablePalettesNotEqual) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Create(FontPalette::kDarkPalette);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Scale(FontPalette::Create(FontPalette::kDarkPalette), 0.1);
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
                       FontPalette::Create(FontPalette::kLightPalette), 0.1);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 0.1);
  scoped_refptr<FontPalette> palette = FontPalette::Add(palette1, palette2);
  EXPECT_TRUE(*palette1.get() == *palette2.get());
}

}  // namespace blink
