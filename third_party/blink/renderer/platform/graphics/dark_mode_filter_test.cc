// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include <optional>

#include "cc/paint/paint_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
namespace {

TEST(DarkModeFilterTest, ApplyDarkModeToColorsAndFlagsWithInvertLightnessLAB) {
  constexpr float kPrecision = 0.00001f;
  DarkModeSettings settings;
  DarkModeFilter filter(settings);
  const SkColor4f color_white_with_alpha =
      SkColor4f::FromColor(SkColorSetARGB(0x80, 0xFF, 0xFF, 0xFF));
  const SkColor4f color_black_with_alpha =
      SkColor4f::FromColor(SkColorSetARGB(0x80, 0x00, 0x00, 0x00));
  const SkColor4f color_dark =
      SkColor4f::FromColor(SkColorSetARGB(0xFF, 0x12, 0x12, 0x12));
  const SkColor4f color_dark_with_alpha =
      SkColor4f::FromColor(SkColorSetARGB(0x80, 0x12, 0x12, 0x12));

  SkColor4f result = filter.InvertColorIfNeeded(
      SkColors::kWhite, DarkModeFilter::ElementRole::kBackground);
  EXPECT_NEAR(color_dark.fR, result.fR, kPrecision);
  EXPECT_NEAR(color_dark.fG, result.fG, kPrecision);
  EXPECT_NEAR(color_dark.fB, result.fB, kPrecision);
  EXPECT_NEAR(color_dark.fA, result.fA, kPrecision);

  result = filter.InvertColorIfNeeded(SkColors::kBlack,
                                      DarkModeFilter::ElementRole::kBackground);
  EXPECT_NEAR(SkColors::kWhite.fR, result.fR, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fG, result.fG, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fB, result.fB, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fA, result.fA, kPrecision);

  result = filter.InvertColorIfNeeded(color_white_with_alpha,
                                      DarkModeFilter::ElementRole::kBackground);
  EXPECT_NEAR(color_dark_with_alpha.fR, result.fR, kPrecision);
  EXPECT_NEAR(color_dark_with_alpha.fG, result.fG, kPrecision);
  EXPECT_NEAR(color_dark_with_alpha.fB, result.fB, kPrecision);
  EXPECT_NEAR(color_dark_with_alpha.fA, result.fA, kPrecision);

  result = filter.InvertColorIfNeeded(SkColors::kBlack,
                                      DarkModeFilter::ElementRole::kSVG);
  EXPECT_NEAR(SkColors::kWhite.fR, result.fR, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fG, result.fG, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fB, result.fB, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fA, result.fA, kPrecision);

  result = filter.InvertColorIfNeeded(SkColors::kWhite,
                                      DarkModeFilter::ElementRole::kSVG);
  EXPECT_NEAR(color_dark.fR, result.fR, kPrecision);
  EXPECT_NEAR(color_dark.fG, result.fG, kPrecision);
  EXPECT_NEAR(color_dark.fB, result.fB, kPrecision);
  EXPECT_NEAR(color_dark.fA, result.fA, kPrecision);

  result = filter.InvertColorIfNeeded(color_black_with_alpha,
                                      DarkModeFilter::ElementRole::kSVG);
  EXPECT_NEAR(color_white_with_alpha.fR, result.fR, kPrecision);
  EXPECT_NEAR(color_white_with_alpha.fG, result.fG, kPrecision);
  EXPECT_NEAR(color_white_with_alpha.fB, result.fB, kPrecision);
  EXPECT_NEAR(color_white_with_alpha.fA, result.fA, kPrecision);

  cc::PaintFlags flags;
  flags.setColor(SkColors::kBlack);
  auto flags_or_nullopt = filter.ApplyToFlagsIfNeeded(
      flags, DarkModeFilter::ElementRole::kBackground, SkColors::kTransparent);
  ASSERT_NE(flags_or_nullopt, std::nullopt);
  result = flags_or_nullopt.value().getColor4f();
  EXPECT_NEAR(SkColors::kWhite.fR, result.fR, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fG, result.fG, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fB, result.fB, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fA, result.fA, kPrecision);
}

TEST(DarkModeFilterTest, ApplyDarkModeToColorsAndFlagsWithContrast) {
  DarkModeSettings settings;
  settings.background_brightness_threshold = 205;
  DarkModeFilter filter(settings);

  const SkColor4f target_for_white =
      SkColor4f::FromColor(SkColorSetRGB(0x12, 0x12, 0x12));
  const SkColor4f target_for_black =
      SkColor4f::FromColor(SkColorSetRGB(0x57, 0x57, 0x57));

  EXPECT_EQ(target_for_white,
            filter.InvertColorIfNeeded(SkColors::kWhite,
                                       DarkModeFilter::ElementRole::kBorder,
                                       SkColors::kBlack));
  EXPECT_EQ(target_for_black,
            filter.InvertColorIfNeeded(SkColors::kBlack,
                                       DarkModeFilter::ElementRole::kBorder,
                                       SkColors::kBlack));

  cc::PaintFlags flags;
  flags.setColor(SkColors::kWhite);
  auto flags_or_nullopt = filter.ApplyToFlagsIfNeeded(
      flags, DarkModeFilter::ElementRole::kBorder, SkColors::kBlack);
  ASSERT_NE(flags_or_nullopt, std::nullopt);
  EXPECT_EQ(target_for_white, flags_or_nullopt.value().getColor4f());
}

// crbug.com/1365680
TEST(DarkModeFilterTest, AdjustDarkenColorDoesNotInfiniteLoop) {
  DarkModeSettings settings;
  settings.foreground_brightness_threshold = 150;
  settings.background_brightness_threshold = 205;
  DarkModeFilter filter(settings);

  const SkColor4f darken_to_black =
      SkColor4f::FromColor(SkColorSetRGB(0x09, 0xe6, 0x0c));
  const SkColor4f high_contrast =
      SkColor4f::FromColor(SkColorSetRGB(0x4c, 0xdc, 0x6d));

  const SkColor4f darken_to_black1 =
      SkColor4f::FromColor(SkColorSetRGB(0x02, 0xd7, 0x72));
  const SkColor4f high_contrast1 =
      SkColor4f::FromColor(SkColorSetRGB(0xcf, 0xea, 0x3b));

  const SkColor4f darken_to_black2 =
      SkColor4f::FromColor(SkColorSetRGB(0x09, 0xe6, 0x0c));
  const SkColor4f high_contrast2 =
      SkColor4f::FromColor(SkColorSetRGB(0x4c, 0xdc, 0x6d));

  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(darken_to_black,
                                       DarkModeFilter::ElementRole::kBorder,
                                       high_contrast));
  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(darken_to_black1,
                                       DarkModeFilter::ElementRole::kBorder,
                                       high_contrast1));
  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(darken_to_black2,
                                       DarkModeFilter::ElementRole::kBorder,
                                       high_contrast2));
}

TEST(DarkModeFilterTest, InvertedColorCacheSize) {
  DarkModeSettings settings;
  DarkModeFilter filter(settings);
  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());

  const SkColor4f color = filter.InvertColorIfNeeded(
      SkColors::kWhite, DarkModeFilter::ElementRole::kBackground);
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
  // Should get cached value.
  EXPECT_EQ(color,
            filter.InvertColorIfNeeded(
                SkColors::kWhite, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
}

TEST(DarkModeFilterTest, InvertedColorCacheZeroMaxKeys) {
  DarkModeSettings settings;
  DarkModeFilter filter(settings);

  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());

  const SkColor4f color1 = filter.InvertColorIfNeeded(
      SkColors::kWhite, DarkModeFilter::ElementRole::kBackground);
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());

  const SkColor4f color2 = filter.InvertColorIfNeeded(
      SkColors::kTransparent, DarkModeFilter::ElementRole::kBackground);
  EXPECT_EQ(2u, filter.GetInvertedColorCacheSizeForTesting());

  // Results returned from cache.
  EXPECT_EQ(color1,
            filter.InvertColorIfNeeded(
                SkColors::kWhite, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(color2, filter.InvertColorIfNeeded(
                        SkColors::kTransparent,
                        DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(2u, filter.GetInvertedColorCacheSizeForTesting());
}

}  // namespace
}  // namespace blink
