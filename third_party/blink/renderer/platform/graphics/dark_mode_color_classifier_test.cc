// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
namespace {

Color GetColorWithBrightness(int target_brightness) {
  CHECK_GE(target_brightness, 0);
  CHECK_LE(target_brightness, 256);

  return Color(target_brightness, target_brightness, target_brightness);
}

TEST(DarkModeColorClassifierTest, ApplyFilterToDarkTextOnly) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  settings.text_brightness_threshold = 200;
  auto classifier = DarkModeColorClassifier::MakeTextColorClassifier(settings);

  // Verify that the following are inverted:
  //   * black text
  //   * text darker than the text brightness threshold
  // and the following are not inverted:
  //   * white text
  //   * text brighter than the text brightness threshold
  //   * text at the brightness threshold
  EXPECT_EQ(DarkModeClassification::kApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.text_brightness_threshold - 5)));
  EXPECT_EQ(DarkModeClassification::kApplyFilter,
            classifier->ShouldInvertColor(Color::kBlack));

  EXPECT_EQ(DarkModeClassification::kDoNotApplyFilter,
            classifier->ShouldInvertColor(Color::kWhite));
  EXPECT_EQ(DarkModeClassification::kDoNotApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.text_brightness_threshold + 5)));
  EXPECT_EQ(DarkModeClassification::kDoNotApplyFilter,
            classifier->ShouldInvertColor(
                GetColorWithBrightness(settings.text_brightness_threshold)));
}

TEST(DarkModeColorClassifierTest, ApplyFilterToLightBackgroundElementsOnly) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  settings.background_brightness_threshold = 200;
  auto classifier =
      DarkModeColorClassifier::MakeBackgroundColorClassifier(settings);

  EXPECT_EQ(DarkModeClassification::kApplyFilter,
            classifier->ShouldInvertColor(Color::kWhite));
  EXPECT_EQ(DarkModeClassification::kDoNotApplyFilter,
            classifier->ShouldInvertColor(Color::kBlack));

  EXPECT_EQ(DarkModeClassification::kApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.background_brightness_threshold + 5)));
  EXPECT_EQ(DarkModeClassification::kDoNotApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.background_brightness_threshold)));
  EXPECT_EQ(DarkModeClassification::kDoNotApplyFilter,
            classifier->ShouldInvertColor(GetColorWithBrightness(
                settings.background_brightness_threshold - 5)));
}

}  // namespace
}  // namespace blink
