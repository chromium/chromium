// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"

#include "base/check_op.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
namespace {

SkColor GetColorWithBrightness(int target_brightness) {
  CHECK_GE(target_brightness, 0);
  CHECK_LE(target_brightness, 256);

  return SkColorSetRGB(target_brightness, target_brightness, target_brightness);
}

TEST(DarkModeColorClassifierTest, ApplyFilterToDarkForegroundOnly) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  settings.foreground_brightness_threshold = 200;
  auto classifier =
      DarkModeColorClassifier::MakeForegroundColorClassifier(settings);

  // Verify that the following are inverted:
  //   * black foreground
  //   * foreground darker than the foreground brightness threshold
  // and the following are not inverted:
  //   * white foreground
  //   * foreground brighter than the foreground brightness threshold
  //   * foreground at the brightness threshold
  EXPECT_EQ(DarkModeResult::kApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.foreground_brightness_threshold - 5)));
  EXPECT_EQ(DarkModeResult::kApplyFilter,
            classifier->ShouldInvertColor(SK_ColorBLACK));

  EXPECT_EQ(DarkModeResult::kDoNotApplyFilter,
            classifier->ShouldInvertColor(SK_ColorWHITE));
  EXPECT_EQ(DarkModeResult::kDoNotApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.foreground_brightness_threshold + 5)));
  EXPECT_EQ(DarkModeResult::kDoNotApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.foreground_brightness_threshold)));
}

TEST(DarkModeColorClassifierTest, ApplyFilterToLightBackgroundElementsOnly) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  settings.background_brightness_threshold = 200;
  auto classifier =
      DarkModeColorClassifier::MakeBackgroundColorClassifier(settings);

  EXPECT_EQ(DarkModeResult::kApplyFilter,
            classifier->ShouldInvertColor(SK_ColorWHITE));
  EXPECT_EQ(DarkModeResult::kDoNotApplyFilter,
            classifier->ShouldInvertColor(SK_ColorBLACK));

  EXPECT_EQ(DarkModeResult::kApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.background_brightness_threshold + 5)));
  EXPECT_EQ(DarkModeResult::kDoNotApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.background_brightness_threshold)));
  EXPECT_EQ(DarkModeResult::kDoNotApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.background_brightness_threshold - 5)));
}

}  // namespace
}  // namespace blink
