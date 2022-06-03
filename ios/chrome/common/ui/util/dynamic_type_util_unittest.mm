// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/dynamic_type_util.h"

#import <UIKit/UIKit.h>

#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for DynamicTypeUtil class.
class CommonDynamicTypeUtilTest : public PlatformTest {
 protected:
  CommonDynamicTypeUtilTest() {}
  ~CommonDynamicTypeUtilTest() override {}

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

// Tests that |PreferredFontForTextStyleWithMaxCategory| works well with various
// input scenarios.
TEST_F(CommonDynamicTypeUtilTest, PreferredFontSize) {
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
