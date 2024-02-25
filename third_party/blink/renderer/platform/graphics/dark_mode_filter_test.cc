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

TEST(DarkModeFilterTest, ApplyDarkModeToColorsAndFlags) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  DarkModeFilter filter(settings);

  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(
                SkColors::kWhite, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(SkColors::kWhite,
            filter.InvertColorIfNeeded(
                SkColors::kBlack, DarkModeFilter::ElementRole::kBackground));

  EXPECT_EQ(SkColors::kWhite,
            filter.InvertColorIfNeeded(SkColors::kBlack,
                                       DarkModeFilter::ElementRole::kSVG));
  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(SkColors::kWhite,
                                       DarkModeFilter::ElementRole::kSVG));

  cc::PaintFlags flags;
  flags.setColor(SkColors::kWhite);
  auto flags_or_nullopt = filter.ApplyToFlagsIfNeeded(
      flags, DarkModeFilter::ElementRole::kBackground, SkColors::kTransparent);
  ASSERT_NE(flags_or_nullopt, std::nullopt);
  EXPECT_EQ(SkColors::kBlack, flags_or_nullopt.value().getColor4f());
}

TEST(DarkModeFilterTest, ApplyDarkModeToColorsAndFlagsWithInvertLightnessLAB) {
  constexpr float kPrecision = 0.00001f;
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kInvertLightnessLAB;
  DarkModeFilter filter(settings);
  const SkColor4f ColorWhiteWithAlpha =
      SkColor4f::FromColor(SkColorSetARGB(0x80, 0xFF, 0xFF, 0xFF));
  const SkColor4f ColorBlackWithAlpha =
      SkColor4f::FromColor(SkColorSetARGB(0x80, 0x00, 0x00, 0x00));
  const SkColor4f ColorDark =
      SkColor4f::FromColor(SkColorSetARGB(0xFF, 0x12, 0x12, 0x12));
  const SkColor4f ColorDarkWithAlpha =
      SkColor4f::FromColor(SkColorSetARGB(0x80, 0x12, 0x12, 0x12));

  SkColor4f result = filter.InvertColorIfNeeded(
      SkColors::kWhite, DarkModeFilter::ElementRole::kBackground);
  EXPECT_NEAR(ColorDark.fR, result.fR, kPrecision);
  EXPECT_NEAR(ColorDark.fG, result.fG, kPrecision);
  EXPECT_NEAR(ColorDark.fB, result.fB, kPrecision);
  EXPECT_NEAR(ColorDark.fA, result.fA, kPrecision);

  result = filter.InvertColorIfNeeded(SkColors::kBlack,
                                      DarkModeFilter::ElementRole::kBackground);
  EXPECT_NEAR(SkColors::kWhite.fR, result.fR, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fG, result.fG, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fB, result.fB, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fA, result.fA, kPrecision);

  result = filter.InvertColorIfNeeded(ColorWhiteWithAlpha,
                                      DarkModeFilter::ElementRole::kBackground);
  EXPECT_NEAR(ColorDarkWithAlpha.fR, result.fR, kPrecision);
  EXPECT_NEAR(ColorDarkWithAlpha.fG, result.fG, kPrecision);
  EXPECT_NEAR(ColorDarkWithAlpha.fB, result.fB, kPrecision);
  EXPECT_NEAR(ColorDarkWithAlpha.fA, result.fA, kPrecision);

  result = filter.InvertColorIfNeeded(SkColors::kBlack,
                                      DarkModeFilter::ElementRole::kSVG);
  EXPECT_NEAR(SkColors::kWhite.fR, result.fR, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fG, result.fG, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fB, result.fB, kPrecision);
  EXPECT_NEAR(SkColors::kWhite.fA, result.fA, kPrecision);

  result = filter.InvertColorIfNeeded(SkColors::kWhite,
                                      DarkModeFilter::ElementRole::kSVG);
  EXPECT_NEAR(ColorDark.fR, result.fR, kPrecision);
  EXPECT_NEAR(ColorDark.fG, result.fG, kPrecision);
  EXPECT_NEAR(ColorDark.fB, result.fB, kPrecision);
  EXPECT_NEAR(ColorDark.fA, result.fA, kPrecision);

  result = filter.InvertColorIfNeeded(ColorBlackWithAlpha,
                                      DarkModeFilter::ElementRole::kSVG);
  EXPECT_NEAR(ColorWhiteWithAlpha.fR, result.fR, kPrecision);
  EXPECT_NEAR(ColorWhiteWithAlpha.fG, result.fG, kPrecision);
  EXPECT_NEAR(ColorWhiteWithAlpha.fB, result.fB, kPrecision);
  EXPECT_NEAR(ColorWhiteWithAlpha.fA, result.fA, kPrecision);

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
  settings.mode = DarkModeInversionAlgorithm::kInvertLightnessLAB;
  settings.background_brightness_threshold = 205;
  DarkModeFilter filter(settings);

  const SkColor4f Target_For_White =
      SkColor4f::FromColor(SkColorSetRGB(0x12, 0x12, 0x12));
  const SkColor4f Target_For_Black =
      SkColor4f::FromColor(SkColorSetRGB(0x57, 0x57, 0x57));

  EXPECT_EQ(Target_For_White,
            filter.InvertColorIfNeeded(SkColors::kWhite,
                                       DarkModeFilter::ElementRole::kBorder,
                                       SkColors::kBlack));
  EXPECT_EQ(Target_For_Black,
            filter.InvertColorIfNeeded(SkColors::kBlack,
                                       DarkModeFilter::ElementRole::kBorder,
                                       SkColors::kBlack));

  cc::PaintFlags flags;
  flags.setColor(SkColors::kWhite);
  auto flags_or_nullopt = filter.ApplyToFlagsIfNeeded(
      flags, DarkModeFilter::ElementRole::kBorder, SkColors::kBlack);
  ASSERT_NE(flags_or_nullopt, std::nullopt);
  EXPECT_EQ(Target_For_White, flags_or_nullopt.value().getColor4f());
}

// crbug.com/1365680
TEST(DarkModeFilterTest, AdjustDarkenColorDoesNotInfiniteLoop) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kInvertLightnessLAB;
  settings.foreground_brightness_threshold = 150;
  settings.background_brightness_threshold = 205;
  DarkModeFilter filter(settings);

  const SkColor4f Darken_To_Black =
      SkColor4f::FromColor(SkColorSetRGB(0x09, 0xe6, 0x0c));
  const SkColor4f High_Contrast =
      SkColor4f::FromColor(SkColorSetRGB(0x4c, 0xdc, 0x6d));

  const SkColor4f Darken_To_Black1 =
      SkColor4f::FromColor(SkColorSetRGB(0x02, 0xd7, 0x72));
  const SkColor4f High_Contrast1 =
      SkColor4f::FromColor(SkColorSetRGB(0xcf, 0xea, 0x3b));

  const SkColor4f Darken_To_Black2 =
      SkColor4f::FromColor(SkColorSetRGB(0x09, 0xe6, 0x0c));
  const SkColor4f High_Contrast2 =
      SkColor4f::FromColor(SkColorSetRGB(0x4c, 0xdc, 0x6d));

  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(Darken_To_Black,
                                       DarkModeFilter::ElementRole::kBorder,
                                       High_Contrast));
  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(Darken_To_Black1,
                                       DarkModeFilter::ElementRole::kBorder,
                                       High_Contrast1));
  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(Darken_To_Black2,
                                       DarkModeFilter::ElementRole::kBorder,
                                       High_Contrast2));
}

TEST(DarkModeFilterTest, InvertedColorCacheSize) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  DarkModeFilter filter(settings);
  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(
                SkColors::kWhite, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
  // Should get cached value.
  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(
                SkColors::kWhite, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
}

TEST(DarkModeFilterTest, InvertedColorCacheZeroMaxKeys) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  DarkModeFilter filter(settings);

  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(
                SkColors::kWhite, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(
      SkColors::kTransparent,
      filter.InvertColorIfNeeded(SkColors::kTransparent,
                                 DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(2u, filter.GetInvertedColorCacheSizeForTesting());

  // Results returned from cache.
  EXPECT_EQ(SkColors::kBlack,
            filter.InvertColorIfNeeded(
                SkColors::kWhite, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(
      SkColors::kTransparent,
      filter.InvertColorIfNeeded(SkColors::kTransparent,
                                 DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(2u, filter.GetInvertedColorCacheSizeForTesting());
}

}  // namespace
}  // namespace blink
