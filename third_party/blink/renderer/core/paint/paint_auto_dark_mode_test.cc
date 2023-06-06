// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class PaintAutoDarkModeTest : public testing::Test {};

TEST_F(PaintAutoDarkModeTest, ShouldApplyFilterToImage) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  settings.image_policy = DarkModeImagePolicy::kFilterSmart;
  DarkModeFilter filter(settings);

  display::ScreenInfo screen_info;
  screen_info.rect = gfx::Rect(1920, 1080);
  screen_info.device_scale_factor = 1.0f;

  // |dst| is smaller than threshold size.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          screen_info, gfx::RectF(50, 50), gfx::RectF(50, 50))));

  // |dst| is smaller than threshold size, even |src| is larger.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          screen_info, gfx::RectF(50, 50), gfx::RectF(200, 200))));

  // |dst| is smaller than threshold size, |src| is smaller.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          screen_info, gfx::RectF(50, 50), gfx::RectF(20, 20))));

  // |src| having very smaller width, even |dst| is larger than threshold size.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          screen_info, gfx::RectF(200, 5), gfx::RectF(200, 5))));

  // |src| having very smaller height, even |dst| is larger than threshold size.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          screen_info, gfx::RectF(5, 200), gfx::RectF(5, 200))));

  // |dst| is larger than threshold size.
  EXPECT_FALSE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          screen_info, gfx::RectF(200, 200), gfx::RectF(20, 20))));

  // |dst| is larger than threshold size.
  EXPECT_FALSE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          screen_info, gfx::RectF(20, 200), gfx::RectF(20, 200))));
}

// Test for mobile display configuration
TEST_F(PaintAutoDarkModeTest, ShouldApplyFilterToImageOnMobile) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  settings.image_policy = DarkModeImagePolicy::kFilterSmart;
  DarkModeFilter filter(settings);

  display::ScreenInfo screen_info;
  screen_info.rect = gfx::Rect(360, 780);
  screen_info.device_scale_factor = 3.0f;

  // 44x44 css image which is above the physical size threshold
  // but with in the device ratio threshold
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          screen_info, gfx::RectF(132, 132), gfx::RectF(132, 132))));

  // 60x60 css image
  EXPECT_FALSE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          screen_info, gfx::RectF(180, 180), gfx::RectF(180, 180))));
}

}  // namespace blink
