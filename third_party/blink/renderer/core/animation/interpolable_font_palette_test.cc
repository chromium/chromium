// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_font_palette.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/values_equivalent.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_palette.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(InterpolableFontPaletteTest, SimpleEndpointsInterpolation) {
  test::TaskEnvironment task_environment;
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Create(FontPalette::kLightPalette);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Create(FontPalette::kDarkPalette);

  InterpolableFontPalette* interpolable_palette_from =
      InterpolableFontPalette::Create(palette1);
  InterpolableFontPalette* interpolable_palette_to =
      InterpolableFontPalette::Create(palette2);

  InterpolableValue* interpolable_value =
      interpolable_palette_from->CloneAndZero();
  interpolable_palette_from->Interpolate(*interpolable_palette_to, 0.3,
                                         *interpolable_value);
  const auto& result_palette = To<InterpolableFontPalette>(*interpolable_value);
  scoped_refptr<const FontPalette> font_palette =
      result_palette.GetFontPalette();

  EXPECT_EQ("palette-mix(in oklab, light, dark 30%)", font_palette->ToString());
}

TEST(InterpolableFontPaletteTest, NestedEndpointsInterpolation) {
  test::TaskEnvironment task_environment;
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Create(FontPalette::kLightPalette);
  scoped_refptr<FontPalette> palette2 = FontPalette::Mix(
      FontPalette::Create(), FontPalette::Create(FontPalette::kDarkPalette), 30,
      70, 0.7, 1.0, Color::ColorSpace::kSRGB, std::nullopt);

  InterpolableFontPalette* interpolable_palette_from =
      InterpolableFontPalette::Create(palette1);
  InterpolableFontPalette* interpolable_palette_to =
      InterpolableFontPalette::Create(palette2);

  InterpolableValue* interpolable_value =
      interpolable_palette_from->CloneAndZero();
  interpolable_palette_from->Interpolate(*interpolable_palette_to, 0.3,
                                         *interpolable_value);
  const auto& result_palette = To<InterpolableFontPalette>(*interpolable_value);
  scoped_refptr<const FontPalette> font_palette =
      result_palette.GetFontPalette();

  EXPECT_EQ(
      "palette-mix(in oklab, light, palette-mix(in srgb, normal, dark 70%) "
      "30%)",
      font_palette->ToString());
}

// Scale/Add should have no effect.
TEST(InterpolableFontPaletteTest, TestScaleAndAdd) {
  test::TaskEnvironment task_environment;
  scoped_refptr<FontPalette> palette1 = FontPalette::Mix(
      FontPalette::Create(), FontPalette::Create(FontPalette::kDarkPalette), 30,
      70, 0.7, 1.0, Color::ColorSpace::kOklab, std::nullopt);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Create(FontPalette::kLightPalette);
  InterpolableFontPalette* interpolable_palette1 =
      InterpolableFontPalette::Create(palette1);
  InterpolableFontPalette* interpolable_palette2 =
      InterpolableFontPalette::Create(palette2);

  interpolable_palette1->Scale(0.5);
  interpolable_palette1->Add(*interpolable_palette2);

  scoped_refptr<const FontPalette> font_palette =
      interpolable_palette1->GetFontPalette();

  EXPECT_TRUE(base::ValuesEquivalent(font_palette,
                                     interpolable_palette2->GetFontPalette()));
}

TEST(InterpolableFontPaletteTest, InterpolablePalettesEqual) {
  test::TaskEnvironment task_environment;
  scoped_refptr<FontPalette> palette1 = FontPalette::Mix(
      FontPalette::Create(FontPalette::kLightPalette), FontPalette::Create(),
      70, 30, 0.3, 1.0, Color::ColorSpace::kOklab, std::nullopt);
  scoped_refptr<FontPalette> palette2 = FontPalette::Mix(
      FontPalette::Create(FontPalette::kLightPalette), FontPalette::Create(),
      70, 30, 0.3, 1.0, Color::ColorSpace::kOklab, std::nullopt);

  InterpolableFontPalette* interpolable_palette1 =
      InterpolableFontPalette::Create(palette1);
  InterpolableFontPalette* interpolable_palette2 =
      InterpolableFontPalette::Create(palette2);

  EXPECT_TRUE(interpolable_palette1->Equals(*interpolable_palette2));
  EXPECT_TRUE(interpolable_palette2->Equals(*interpolable_palette1));
}

TEST(InterpolableFontPaletteTest, InterpolablePalettesNotEqual) {
  test::TaskEnvironment task_environment;
  scoped_refptr<FontPalette> palette1 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kLightPalette),
                       FontPalette::Create(FontPalette::kDarkPalette), 70, 30,
                       0.3, 1.0, Color::ColorSpace::kSRGB, std::nullopt);
  scoped_refptr<FontPalette> palette2 =
      FontPalette::Mix(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette), 70, 30,
                       0.3, 1.0, Color::ColorSpace::kSRGB, std::nullopt);

  InterpolableFontPalette* interpolable_palette1 =
      InterpolableFontPalette::Create(palette1);
  InterpolableFontPalette* interpolable_palette2 =
      InterpolableFontPalette::Create(palette2);

  EXPECT_FALSE(interpolable_palette1->Equals(*interpolable_palette2));
  EXPECT_FALSE(interpolable_palette2->Equals(*interpolable_palette1));
}

}  // namespace blink
