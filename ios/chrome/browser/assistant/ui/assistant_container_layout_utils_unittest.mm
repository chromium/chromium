// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using AssistantContainerLayoutUtilsTest = PlatformTest;

// Tests the morphology constraints output when all detents are defined.
TEST_F(AssistantContainerLayoutUtilsTest, CalculateConstraints_AllDetents) {
  NSInteger minimized = 100;
  NSInteger medium = 300;
  NSInteger large = 600;

  // Below minimized (rubber banding constraints).
  auto constraints = CalculateMorphingConstraints(50, minimized, medium, large);
  EXPECT_EQ(100.0, constraints.actual_height);
  EXPECT_EQ(kMorphingBaseMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingBaseMargin - 50.0, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);

  // Exactly at minimized.
  constraints = CalculateMorphingConstraints(100, minimized, medium, large);
  EXPECT_EQ(100.0, constraints.actual_height);
  EXPECT_EQ(kMorphingBaseMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingBaseMargin, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);

  // Between minimized and medium at progress 0.5.
  constraints = CalculateMorphingConstraints(200, minimized, medium, large);
  EXPECT_EQ(200.0, constraints.actual_height);
  EXPECT_EQ(
      kMorphingBaseMargin + (kMorphingMediumMargin - kMorphingBaseMargin) * 0.5,
      constraints.side_margin);
  EXPECT_EQ(
      kMorphingBaseMargin + (kMorphingMediumMargin - kMorphingBaseMargin) * 0.5,
      constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(
      kMorphingBaseCornerRadius +
          (kMorphingMediumBottomCornerRadius - kMorphingBaseCornerRadius) * 0.5,
      constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);

  // Exactly at medium.
  constraints = CalculateMorphingConstraints(300, minimized, medium, large);
  EXPECT_EQ(300.0, constraints.actual_height);
  EXPECT_EQ(kMorphingMediumMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingMediumMargin, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingMediumBottomCornerRadius,
            constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);

  // Between medium and large at progress 0.5.
  CGFloat progress = 0.5;
  constraints = CalculateMorphingConstraints(450, minimized, medium, large);
  EXPECT_EQ(450.0, constraints.actual_height);
  EXPECT_EQ(kMorphingMediumMargin + (0.0 - kMorphingMediumMargin) * progress,
            constraints.side_margin);
  EXPECT_EQ(kMorphingMediumMargin + (0.0 - kMorphingMediumMargin) * progress,
            constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingMediumBottomCornerRadius * 0.5,
            constraints.bottom_corner_radius);
  EXPECT_EQ(progress * kMaxBackgroundDimmingAlpha,
            constraints.background_dimming_alpha);

  // Exactly at large (bottom margin reaches 0, bottom mask drops).
  constraints = CalculateMorphingConstraints(600, minimized, medium, large);
  EXPECT_EQ(600.0, constraints.actual_height);
  EXPECT_EQ(0.0, constraints.side_margin);
  EXPECT_EQ(0.0, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(0.0, constraints.bottom_corner_radius);
  EXPECT_EQ(kMaxBackgroundDimmingAlpha, constraints.background_dimming_alpha);
}

// Tests behavior bypassing medium explicitly.
TEST_F(AssistantContainerLayoutUtilsTest, CalculateConstraints_NoMediumDetent) {
  NSInteger minimized = 100;
  NSInteger medium = -1;
  NSInteger large = 600;

  // Between minimized and large at progress 0.5.
  CGFloat progress = 0.5;
  auto constraints =
      CalculateMorphingConstraints(350, minimized, medium, large);
  EXPECT_EQ(350.0, constraints.actual_height);
  EXPECT_EQ(kMorphingBaseMargin + (0.0 - kMorphingBaseMargin) * progress,
            constraints.side_margin);
  EXPECT_EQ(kMorphingBaseMargin + (0.0 - kMorphingBaseMargin) * progress,
            constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingBaseCornerRadius * 0.5, constraints.bottom_corner_radius);
  EXPECT_EQ(progress * kMaxBackgroundDimmingAlpha,
            constraints.background_dimming_alpha);
}

// Tests behavior bypassing large explicitly.
TEST_F(AssistantContainerLayoutUtilsTest, CalculateConstraints_NoLargeDetent) {
  NSInteger minimized = 100;
  NSInteger medium = 300;
  NSInteger large = -1;

  // Exceeding the top-most available detent (medium) should lock to medium
  // properties.
  auto constraints =
      CalculateMorphingConstraints(450, minimized, medium, large);
  EXPECT_EQ(450.0, constraints.actual_height);
  EXPECT_EQ(kMorphingMediumMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingMediumMargin, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingMediumBottomCornerRadius,
            constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);
}

// Tests behavior bypassing minimized explicitly.
TEST_F(AssistantContainerLayoutUtilsTest,
       CalculateConstraints_NoMinimizedDetent) {
  NSInteger minimized = -1;
  NSInteger medium = 300;
  NSInteger large = 600;

  // Below the lowest available detent (medium) should lock to medium
  // properties.
  auto constraints =
      CalculateMorphingConstraints(150, minimized, medium, large);
  EXPECT_EQ(300.0, constraints.actual_height);
  EXPECT_EQ(kMorphingMediumMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingMediumMargin - 150.0, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingMediumBottomCornerRadius,
            constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);
}

// Tests an isolated minimized detent.
TEST_F(AssistantContainerLayoutUtilsTest,
       CalculateConstraints_OnlyMinimizedDetent) {
  NSInteger minimized = 100;
  NSInteger medium = -1;
  NSInteger large = -1;

  // Exceeding the only available detent (minimized) should keep properties
  // locked.
  auto constraints =
      CalculateMorphingConstraints(800, minimized, medium, large);
  EXPECT_EQ(800.0, constraints.actual_height);
  EXPECT_EQ(kMorphingBaseMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingBaseMargin, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);
}

// Tests an isolated medium detent.
TEST_F(AssistantContainerLayoutUtilsTest,
       CalculateConstraints_OnlyMediumDetent) {
  NSInteger minimized = -1;
  NSInteger medium = 300;
  NSInteger large = -1;

  // Values should always remain explicitly glued to medium properties.
  auto constraints =
      CalculateMorphingConstraints(150, minimized, medium, large);
  EXPECT_EQ(300.0, constraints.actual_height);
  EXPECT_EQ(kMorphingMediumMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingMediumMargin - 150.0, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingMediumBottomCornerRadius,
            constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);

  constraints = CalculateMorphingConstraints(800, minimized, medium, large);
  EXPECT_EQ(800.0, constraints.actual_height);
  EXPECT_EQ(kMorphingMediumMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingMediumMargin, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingMediumBottomCornerRadius,
            constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);
}

// Tests an isolated large detent.
TEST_F(AssistantContainerLayoutUtilsTest,
       CalculateConstraints_OnlyLargeDetent) {
  NSInteger minimized = -1;
  NSInteger medium = -1;
  NSInteger large = 600;

  // Below the only available detent (large) should strictly reduce height.
  auto constraints =
      CalculateMorphingConstraints(150, minimized, medium, large);
  EXPECT_EQ(150.0, constraints.actual_height);
  EXPECT_EQ(0.0, constraints.side_margin);
  EXPECT_EQ(0.0, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(0.0, constraints.bottom_corner_radius);
  EXPECT_EQ(kMaxBackgroundDimmingAlpha, constraints.background_dimming_alpha);
}
