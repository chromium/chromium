// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include "cc/paint/paint_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
namespace {

TEST(DarkModeFilterTest, ApplyDarkModeToColorsAndFlags) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  DarkModeFilter filter(settings);

  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(SK_ColorWHITE,
            filter.InvertColorIfNeeded(
                SK_ColorBLACK, DarkModeFilter::ElementRole::kBackground));

  EXPECT_EQ(SK_ColorWHITE,
            filter.InvertColorIfNeeded(SK_ColorBLACK,
                                       DarkModeFilter::ElementRole::kSVG));
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(SK_ColorWHITE,
                                       DarkModeFilter::ElementRole::kSVG));

  cc::PaintFlags flags;
  flags.setColor(SK_ColorWHITE);
  auto flags_or_nullopt = filter.ApplyToFlagsIfNeeded(
      flags, DarkModeFilter::ElementRole::kBackground);
  ASSERT_NE(flags_or_nullopt, absl::nullopt);
  EXPECT_EQ(SK_ColorBLACK, flags_or_nullopt.value().getColor());
}

TEST(DarkModeFilterTest, ApplyDarkModeToColorsAndFlagsWithInvertLightnessLAB) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kInvertLightnessLAB;
  DarkModeFilter filter(settings);
  constexpr SkColor SK_ColorWhiteWithAlpha =
      SkColorSetARGB(0x80, 0xFF, 0xFF, 0xFF);
  constexpr SkColor SK_ColorBlackWithAlpha =
      SkColorSetARGB(0x80, 0x00, 0x00, 0x00);
  constexpr SkColor SK_ColorDark = SkColorSetARGB(0xFF, 0x12, 0x12, 0x12);
  constexpr SkColor SK_ColorDarkWithAlpha =
      SkColorSetARGB(0x80, 0x12, 0x12, 0x12);

  EXPECT_EQ(SK_ColorDark,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(SK_ColorWHITE,
            filter.InvertColorIfNeeded(
                SK_ColorBLACK, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(
      SK_ColorDarkWithAlpha,
      filter.InvertColorIfNeeded(SK_ColorWhiteWithAlpha,
                                 DarkModeFilter::ElementRole::kBackground));

  EXPECT_EQ(SK_ColorWHITE,
            filter.InvertColorIfNeeded(SK_ColorBLACK,
                                       DarkModeFilter::ElementRole::kSVG));
  EXPECT_EQ(SK_ColorDark,
            filter.InvertColorIfNeeded(SK_ColorWHITE,
                                       DarkModeFilter::ElementRole::kSVG));
  EXPECT_EQ(SK_ColorWhiteWithAlpha,
            filter.InvertColorIfNeeded(SK_ColorBlackWithAlpha,
                                       DarkModeFilter::ElementRole::kSVG));

  cc::PaintFlags flags;
  flags.setColor(SK_ColorBLACK);
  auto flags_or_nullopt = filter.ApplyToFlagsIfNeeded(
      flags, DarkModeFilter::ElementRole::kBackground);
  ASSERT_NE(flags_or_nullopt, absl::nullopt);
  EXPECT_EQ(SK_ColorWHITE, flags_or_nullopt.value().getColor());
}

TEST(DarkModeFilterTest, InvertedColorCacheSize) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  DarkModeFilter filter(settings);
  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
  // Should get cached value.
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
}

TEST(DarkModeFilterTest, InvertedColorCacheZeroMaxKeys) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  DarkModeFilter filter(settings);

  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SK_ColorTRANSPARENT,
            filter.InvertColorIfNeeded(
                SK_ColorTRANSPARENT, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(2u, filter.GetInvertedColorCacheSizeForTesting());

  // Results returned from cache.
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(SK_ColorTRANSPARENT,
            filter.InvertColorIfNeeded(
                SK_ColorTRANSPARENT, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(2u, filter.GetInvertedColorCacheSizeForTesting());
}

}  // namespace
}  // namespace blink
