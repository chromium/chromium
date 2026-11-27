// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using AssistantContainerLayoutUtilsTest = PlatformTest;

// Tests the morphology constraints output when all detents are defined.
TEST_F(AssistantContainerLayoutUtilsTest, CalculateConstraints_AllDetents) {
  NSInteger minimized = 100;
  NSInteger medium = 300;
  NSInteger large = 600;

  // Below minimized (rubber banding constraints).
  auto constraints =
      CalculateMorphingConstraints(50, minimized, medium, large, medium);
  EXPECT_EQ(100.0, constraints.actual_height);
  EXPECT_EQ(kMorphingBaseMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingBaseMargin - 50.0, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);

  // Exactly at minimized.
  constraints =
      CalculateMorphingConstraints(100, minimized, medium, large, medium);
  EXPECT_EQ(100.0, constraints.actual_height);
  EXPECT_EQ(kMorphingBaseMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingBaseMargin, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);

  // Between minimized and medium at progress 0.5.
  constraints =
      CalculateMorphingConstraints(200, minimized, medium, large, medium);
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
  constraints =
      CalculateMorphingConstraints(300, minimized, medium, large, medium);
  EXPECT_EQ(300.0, constraints.actual_height);
  EXPECT_EQ(kMorphingMediumMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingMediumMargin, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingMediumBottomCornerRadius,
            constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);

  // Between medium and large at progress 0.5.
  CGFloat progress = 0.5;
  constraints =
      CalculateMorphingConstraints(450, minimized, medium, large, medium);
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
  constraints =
      CalculateMorphingConstraints(600, minimized, medium, large, medium);
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
      CalculateMorphingConstraints(350, minimized, medium, large, medium);
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
      CalculateMorphingConstraints(450, minimized, medium, large, medium);
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
      CalculateMorphingConstraints(150, minimized, medium, large, medium);
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
      CalculateMorphingConstraints(800, minimized, medium, large, medium);
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
      CalculateMorphingConstraints(150, minimized, medium, large, medium);
  EXPECT_EQ(300.0, constraints.actual_height);
  EXPECT_EQ(kMorphingMediumMargin, constraints.side_margin);
  EXPECT_EQ(kMorphingMediumMargin - 150.0, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(kMorphingMediumBottomCornerRadius,
            constraints.bottom_corner_radius);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);

  constraints =
      CalculateMorphingConstraints(800, minimized, medium, large, medium);
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
      CalculateMorphingConstraints(150, minimized, medium, large, medium);
  EXPECT_EQ(150.0, constraints.actual_height);
  EXPECT_EQ(0.0, constraints.side_margin);
  EXPECT_EQ(0.0, constraints.bottom_margin);
  EXPECT_EQ(kMorphingBaseCornerRadius, constraints.top_corner_radius);
  EXPECT_EQ(0.0, constraints.bottom_corner_radius);
  EXPECT_EQ(kMaxBackgroundDimmingAlpha, constraints.background_dimming_alpha);
}

// Tests dimming constraints when largest_undimmed_detent_height is minimized or
// large.
TEST_F(AssistantContainerLayoutUtilsTest,
       CalculateConstraints_LargestUndimmedDetentVariants) {
  NSInteger minimized = 100;
  NSInteger medium = 300;
  NSInteger large = 600;

  // When largest_undimmed_detent_height is minimized, dimming starts after
  // minimized and reaches max at medium.
  auto constraints =
      CalculateMorphingConstraints(100, minimized, medium, large, minimized);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);

  constraints =
      CalculateMorphingConstraints(200, minimized, medium, large, minimized);
  EXPECT_EQ(0.5 * kMaxBackgroundDimmingAlpha,
            constraints.background_dimming_alpha);

  constraints =
      CalculateMorphingConstraints(300, minimized, medium, large, minimized);
  EXPECT_EQ(kMaxBackgroundDimmingAlpha, constraints.background_dimming_alpha);

  constraints =
      CalculateMorphingConstraints(450, minimized, medium, large, minimized);
  EXPECT_EQ(kMaxBackgroundDimmingAlpha, constraints.background_dimming_alpha);

  // When largest_undimmed_detent_height is large, no detent is dimmed.
  constraints =
      CalculateMorphingConstraints(600, minimized, medium, large, large);
  EXPECT_EQ(0.0, constraints.background_dimming_alpha);
}

// Tests that `ApplyAssistantSidePanelAesthetics` applies the light mode border
// and drop shadow when active, and falls back to light mode for nil traits.
TEST_F(AssistantContainerLayoutUtilsTest, ApplySidePanelAesthetics_LightMode) {
  UIView* content_view = [[UIView alloc] init];
  UIView* shadow_view = [[UIView alloc] init];
  UITraitCollection* light_traits = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleLight];
  UIColor* expected_shadow_background =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  ASSERT_TRUE(expected_shadow_background);

  ApplyAssistantSidePanelAesthetics(content_view, shadow_view, light_traits,
                                    /*active=*/true);

  EXPECT_EQ(kAssistantSidePanelCornerRadius, content_view.layer.cornerRadius);
  EXPECT_GT(content_view.layer.borderWidth, 0.0);
  ASSERT_NE(nullptr, content_view.layer.borderColor);
  EXPECT_EQ(kAssistantSidePanelCornerRadius, shadow_view.layer.cornerRadius);
  EXPECT_NSEQ(expected_shadow_background, shadow_view.backgroundColor);
  EXPECT_TRUE(CGColorEqualToColor(shadow_view.layer.shadowColor,
                                  UIColor.blackColor.CGColor));
  EXPECT_GT(shadow_view.layer.shadowOpacity, 0.0f);

  // Nil traits fall back safely to light mode styling.
  UIView* fallback_content_view = [[UIView alloc] init];
  UIView* fallback_shadow_view = [[UIView alloc] init];
  ApplyAssistantSidePanelAesthetics(fallback_content_view, fallback_shadow_view,
                                    nil, /*active=*/true);
  EXPECT_TRUE(CGColorEqualToColor(fallback_content_view.layer.borderColor,
                                  content_view.layer.borderColor));
  EXPECT_FLOAT_EQ(shadow_view.layer.shadowOpacity,
                  fallback_shadow_view.layer.shadowOpacity);
}

// Tests that `ApplyAssistantSidePanelAesthetics` transitions cleanly between
// light mode and dark mode, hiding the drop shadow and using `kSeparatorColor`
// for the border in dark mode.
TEST_F(AssistantContainerLayoutUtilsTest,
       ApplySidePanelAesthetics_DarkModeTransition) {
  UIView* content_view = [[UIView alloc] init];
  UIView* shadow_view = [[UIView alloc] init];
  UITraitCollection* light_traits = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleLight];
  UITraitCollection* dark_traits = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleDark];
  UIColor* expected_dark_border = [[UIColor colorNamed:kSeparatorColor]
      resolvedColorWithTraitCollection:dark_traits];
  ASSERT_TRUE(expected_dark_border);

  // Start in light mode so transitioning to dark mode verifies active clearing.
  ApplyAssistantSidePanelAesthetics(content_view, shadow_view, light_traits,
                                    /*active=*/true);
  ASSERT_GT(shadow_view.layer.shadowOpacity, 0.0f);
  ASSERT_NE(nullptr, content_view.layer.borderColor);
  UIColor* light_border =
      [UIColor colorWithCGColor:content_view.layer.borderColor];

  // Switch to dark mode: border updates to `kSeparatorColor` and shadow hides.
  ApplyAssistantSidePanelAesthetics(content_view, shadow_view, dark_traits,
                                    /*active=*/true);
  EXPECT_EQ(kAssistantSidePanelCornerRadius, content_view.layer.cornerRadius);
  EXPECT_GT(content_view.layer.borderWidth, 0.0);
  ASSERT_NE(nullptr, content_view.layer.borderColor);
  EXPECT_TRUE(CGColorEqualToColor(content_view.layer.borderColor,
                                  expected_dark_border.CGColor));
  EXPECT_FALSE(CGColorEqualToColor(content_view.layer.borderColor,
                                   light_border.CGColor));
  EXPECT_FLOAT_EQ(0.0f, shadow_view.layer.shadowOpacity);

  // Switch back to light mode: border and shadow are restored.
  ApplyAssistantSidePanelAesthetics(content_view, shadow_view, light_traits,
                                    /*active=*/true);
  EXPECT_TRUE(CGColorEqualToColor(content_view.layer.borderColor,
                                  light_border.CGColor));
  EXPECT_GT(shadow_view.layer.shadowOpacity, 0.0f);
}

// Tests that `ApplyAssistantSidePanelAesthetics` resets corner, border, and
// shadow properties when inactive.
TEST_F(AssistantContainerLayoutUtilsTest,
       ApplySidePanelAesthetics_InactiveResetsProperties) {
  UIView* content_view = [[UIView alloc] init];
  UIView* shadow_view = [[UIView alloc] init];
  UITraitCollection* light_traits = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleLight];

  ApplyAssistantSidePanelAesthetics(content_view, shadow_view, light_traits,
                                    /*active=*/true);
  ApplyAssistantSidePanelAesthetics(content_view, shadow_view, light_traits,
                                    /*active=*/false);

  EXPECT_EQ(0.0, content_view.layer.cornerRadius);
  EXPECT_EQ(0.0, content_view.layer.borderWidth);
  EXPECT_EQ(nullptr, content_view.layer.borderColor);
  EXPECT_EQ(0.0, shadow_view.layer.cornerRadius);
  EXPECT_FLOAT_EQ(0.0f, shadow_view.layer.shadowOpacity);
  EXPECT_NSEQ(UIColor.clearColor, shadow_view.backgroundColor);
}
