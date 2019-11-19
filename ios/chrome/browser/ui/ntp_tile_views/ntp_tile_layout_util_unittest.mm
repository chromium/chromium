// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_layout_util.h"

#import <UIKit/UIKit.h>

#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using NtpTileLayoutUtilTest = PlatformTest;

// Tests that MostVisitedCellSize returns correct size for all content size
// categories.
TEST_F(NtpTileLayoutUtilTest, MostVisitedCellSize) {
  EXPECT_TRUE(
      CGSizeEqualToSize(kNtpTileViewSizeSmall,
                        MostVisitedCellSize(UIContentSizeCategoryUnspecified)));
  EXPECT_TRUE(
      CGSizeEqualToSize(kNtpTileViewSizeSmall,
                        MostVisitedCellSize(UIContentSizeCategoryExtraSmall)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kNtpTileViewSizeSmall, MostVisitedCellSize(UIContentSizeCategorySmall)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kNtpTileViewSizeSmall, MostVisitedCellSize(UIContentSizeCategoryMedium)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kNtpTileViewSizeSmall, MostVisitedCellSize(UIContentSizeCategoryLarge)));
  EXPECT_TRUE(
      CGSizeEqualToSize(kNtpTileViewSizeSmall,
                        MostVisitedCellSize(UIContentSizeCategoryExtraLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kNtpTileViewSizeSmall,
      MostVisitedCellSize(UIContentSizeCategoryExtraExtraLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kNtpTileViewSizeMedium,
      MostVisitedCellSize(UIContentSizeCategoryExtraExtraExtraLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kNtpTileViewSizeLarge,
      MostVisitedCellSize(UIContentSizeCategoryAccessibilityMedium)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kNtpTileViewSizeExtraLarge,
      MostVisitedCellSize(UIContentSizeCategoryAccessibilityLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kNtpTileViewSizeExtraLarge,
      MostVisitedCellSize(UIContentSizeCategoryAccessibilityExtraLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kNtpTileViewSizeExtraLarge,
      MostVisitedCellSize(UIContentSizeCategoryAccessibilityExtraExtraLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kNtpTileViewSizeExtraLarge,
      MostVisitedCellSize(
          UIContentSizeCategoryAccessibilityExtraExtraExtraLarge)));
}

// Tests that CenteredTilesMarginForWidth works under various environment.
TEST_F(NtpTileLayoutUtilTest, CenteredTilesMarginForWidth) {
  // Set up Regular size class and Large font size.
  UITraitCollection* trait_collection =
      [UITraitCollection traitCollectionWithTraitsFromCollections:@[
        [UITraitCollection traitCollectionWithHorizontalSizeClass:
                               UIUserInterfaceSizeClassRegular],
        [UITraitCollection traitCollectionWithPreferredContentSizeCategory:
                               UIContentSizeCategoryLarge]
      ]];

  // Display 4 columns on very big screen.
  EXPECT_EQ(200, CenteredTilesMarginForWidth(
                     trait_collection,
                     kNtpTileViewSizeSmall.width * 4 +
                         kNtpTilesHorizontalSpacingRegular * 3 + 200 * 2));
  // Display 4 columns on normal screen.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kNtpTileViewSizeSmall.width * 4 +
                        kNtpTilesHorizontalSpacingRegular * 3 + 20 * 2));
  // Display 3 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kNtpTileViewSizeSmall.width * 3 +
                        kNtpTilesHorizontalSpacingRegular * 2 + 20 * 2));
  // Display 2 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kNtpTileViewSizeSmall.width * 2 +
                        kNtpTilesHorizontalSpacingRegular * 1 + 20 * 2));
  // Display 1 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kNtpTileViewSizeSmall.width * 1 +
                        kNtpTilesHorizontalSpacingRegular * 0 + 20 * 2));

  // Set up Compact size class and Accessibility Large font size.
  trait_collection =
      [UITraitCollection traitCollectionWithTraitsFromCollections:@[
        [UITraitCollection traitCollectionWithHorizontalSizeClass:
                               UIUserInterfaceSizeClassCompact],
        [UITraitCollection traitCollectionWithPreferredContentSizeCategory:
                               UIContentSizeCategoryAccessibilityLarge]
      ]];

  // Display 4 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kNtpTileViewSizeExtraLarge.width * 4 +
                        kNtpTilesHorizontalSpacingCompact * 3 + 20 * 2));
  // Display 3 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kNtpTileViewSizeExtraLarge.width * 3 +
                        kNtpTilesHorizontalSpacingCompact * 2 + 20 * 2));
  // Display 2 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kNtpTileViewSizeExtraLarge.width * 2 +
                        kNtpTilesHorizontalSpacingCompact * 1 + 20 * 2));
  // Display 1 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kNtpTileViewSizeExtraLarge.width * 1 +
                        kNtpTilesHorizontalSpacingCompact * 0 + 20 * 2));
}
