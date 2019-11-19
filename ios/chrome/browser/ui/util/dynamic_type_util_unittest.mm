// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/util/dynamic_type_util.h"

#import <UIKit/UIKit.h>

#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for DynamicTypeUtil class.
class DynamicTypeUtilTest : public PlatformTest {
 protected:
  DynamicTypeUtilTest() {}
  ~DynamicTypeUtilTest() override {}

  UIFont* PreferredFontForTextStyleAndSizeCategory(
      UIFontTextStyle style,
      UIContentSizeCategory category) {
    return
        [UIFont preferredFontForTextStyle:style
            compatibleWithTraitCollection:
                [UITraitCollection
                    traitCollectionWithPreferredContentSizeCategory:category]];
  }
};

// Checks that the font sizes associated with the "body"
// preferredContentSizeCategory aren't changing in new iOS releases.
TEST_F(DynamicTypeUtilTest, TestFontSize) {
  EXPECT_EQ(14.f, PreferredFontForTextStyleAndSizeCategory(
                      UIFontTextStyleBody, UIContentSizeCategoryExtraSmall)
                      .pointSize);
  EXPECT_EQ(15.f, PreferredFontForTextStyleAndSizeCategory(
                      UIFontTextStyleBody, UIContentSizeCategorySmall)
                      .pointSize);
  EXPECT_EQ(16.f, PreferredFontForTextStyleAndSizeCategory(
                      UIFontTextStyleBody, UIContentSizeCategoryMedium)
                      .pointSize);
  EXPECT_EQ(17.f, PreferredFontForTextStyleAndSizeCategory(
                      UIFontTextStyleBody, UIContentSizeCategoryLarge)
                      .pointSize);
  EXPECT_EQ(19.f, PreferredFontForTextStyleAndSizeCategory(
                      UIFontTextStyleBody, UIContentSizeCategoryExtraLarge)
                      .pointSize);
  EXPECT_EQ(21.f, PreferredFontForTextStyleAndSizeCategory(
                      UIFontTextStyleBody, UIContentSizeCategoryExtraExtraLarge)
                      .pointSize);
  EXPECT_EQ(23.f,
            PreferredFontForTextStyleAndSizeCategory(
                UIFontTextStyleBody, UIContentSizeCategoryExtraExtraExtraLarge)
                .pointSize);
  EXPECT_EQ(28.f,
            PreferredFontForTextStyleAndSizeCategory(
                UIFontTextStyleBody, UIContentSizeCategoryAccessibilityMedium)
                .pointSize);
  EXPECT_EQ(33.f,
            PreferredFontForTextStyleAndSizeCategory(
                UIFontTextStyleBody, UIContentSizeCategoryAccessibilityLarge)
                .pointSize);
  EXPECT_EQ(40.f, PreferredFontForTextStyleAndSizeCategory(
                      UIFontTextStyleBody,
                      UIContentSizeCategoryAccessibilityExtraLarge)
                      .pointSize);
  EXPECT_EQ(47.f, PreferredFontForTextStyleAndSizeCategory(
                      UIFontTextStyleBody,
                      UIContentSizeCategoryAccessibilityExtraExtraLarge)
                      .pointSize);
  EXPECT_EQ(53.f, PreferredFontForTextStyleAndSizeCategory(
                      UIFontTextStyleBody,
                      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge)
                      .pointSize);
}

// Tests that the clamped version of the font size multipler is working.
TEST_F(DynamicTypeUtilTest, TestClampedFontSize) {
  // Test that the bigger categories are truncated.
  float multiplier = SystemSuggestedFontSizeMultiplier(
      UIContentSizeCategoryAccessibilityExtraExtraLarge,
      UIContentSizeCategoryLarge, UIContentSizeCategoryExtraExtraLarge);
  EXPECT_EQ(
      SystemSuggestedFontSizeMultiplier(UIContentSizeCategoryExtraExtraLarge),
      multiplier);

  // Test that the smallest categories are truncated.
  multiplier = SystemSuggestedFontSizeMultiplier(
      UIContentSizeCategoryExtraSmall, UIContentSizeCategoryLarge,
      UIContentSizeCategoryExtraExtraLarge);
  EXPECT_EQ(SystemSuggestedFontSizeMultiplier(UIContentSizeCategoryLarge),
            multiplier);

  // Test that the categories in the range are unchanged.
  multiplier = SystemSuggestedFontSizeMultiplier(
      UIContentSizeCategoryExtraLarge, UIContentSizeCategoryLarge,
      UIContentSizeCategoryExtraExtraLarge);
  EXPECT_EQ(SystemSuggestedFontSizeMultiplier(UIContentSizeCategoryExtraLarge),
            multiplier);

  // Test that the categories on the border of the range are unchanged.
  multiplier = SystemSuggestedFontSizeMultiplier(
      UIContentSizeCategoryLarge, UIContentSizeCategoryLarge,
      UIContentSizeCategoryExtraExtraLarge);
  EXPECT_EQ(SystemSuggestedFontSizeMultiplier(UIContentSizeCategoryLarge),
            multiplier);

  // Test that the categories on the border of the range are unchanged.
  multiplier = SystemSuggestedFontSizeMultiplier(
      UIContentSizeCategoryExtraExtraLarge, UIContentSizeCategoryLarge,
      UIContentSizeCategoryExtraExtraLarge);
  EXPECT_EQ(
      SystemSuggestedFontSizeMultiplier(UIContentSizeCategoryExtraExtraLarge),
      multiplier);
}

// Tests that |PreferredFontForTextStyleWithMaxCategory| works well with various
// input scenarios.
TEST_F(DynamicTypeUtilTest, PreferredFontSize) {
  // Use normal category as maxmium category.
  EXPECT_NSEQ(PreferredFontForTextStyleAndSizeCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryExtraSmall),
              PreferredFontForTextStyleWithMaxCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryExtraSmall,
                  UIContentSizeCategoryMedium));

  EXPECT_NSEQ(PreferredFontForTextStyleAndSizeCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryMedium),
              PreferredFontForTextStyleWithMaxCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryMedium,
                  UIContentSizeCategoryMedium));

  EXPECT_NSEQ(PreferredFontForTextStyleAndSizeCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryMedium),
              PreferredFontForTextStyleWithMaxCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryExtraExtraLarge,
                  UIContentSizeCategoryMedium));

  EXPECT_NSEQ(PreferredFontForTextStyleAndSizeCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryMedium),
              PreferredFontForTextStyleWithMaxCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryAccessibilityLarge,
                  UIContentSizeCategoryMedium));

  // Use accessibility category as maxmium category.
  EXPECT_NSEQ(PreferredFontForTextStyleAndSizeCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryExtraSmall),
              PreferredFontForTextStyleWithMaxCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryExtraSmall,
                  UIContentSizeCategoryAccessibilityLarge));

  EXPECT_NSEQ(PreferredFontForTextStyleAndSizeCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryMedium),
              PreferredFontForTextStyleWithMaxCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryMedium,
                  UIContentSizeCategoryAccessibilityLarge));

  EXPECT_NSEQ(PreferredFontForTextStyleAndSizeCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryAccessibilityLarge),
              PreferredFontForTextStyleWithMaxCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryAccessibilityLarge,
                  UIContentSizeCategoryAccessibilityLarge));

  EXPECT_NSEQ(PreferredFontForTextStyleAndSizeCategory(
                  UIFontTextStyleBody, UIContentSizeCategoryAccessibilityLarge),
              PreferredFontForTextStyleWithMaxCategory(
                  UIFontTextStyleBody,
                  UIContentSizeCategoryAccessibilityExtraExtraLarge,
                  UIContentSizeCategoryAccessibilityLarge));
}
