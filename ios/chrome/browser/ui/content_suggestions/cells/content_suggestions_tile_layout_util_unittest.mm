// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"

#import <UIKit/UIKit.h>

#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using ContentSuggestionsTileLayoutUtilTest = PlatformTest;

// Tests that MostVisitedCellSize returns correct size for all content size
// categories.
TEST_F(ContentSuggestionsTileLayoutUtilTest, MostVisitedCellSize) {
  EXPECT_TRUE(
      CGSizeEqualToSize(kContentSuggestionsTileViewSizeSmall,
                        MostVisitedCellSize(UIContentSizeCategoryUnspecified)));
  EXPECT_TRUE(
      CGSizeEqualToSize(kContentSuggestionsTileViewSizeSmall,
                        MostVisitedCellSize(UIContentSizeCategoryExtraSmall)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kContentSuggestionsTileViewSizeSmall, MostVisitedCellSize(UIContentSizeCategorySmall)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kContentSuggestionsTileViewSizeSmall, MostVisitedCellSize(UIContentSizeCategoryMedium)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kContentSuggestionsTileViewSizeSmall, MostVisitedCellSize(UIContentSizeCategoryLarge)));
  EXPECT_TRUE(
      CGSizeEqualToSize(kContentSuggestionsTileViewSizeMedium,
                        MostVisitedCellSize(UIContentSizeCategoryExtraLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kContentSuggestionsTileViewSizeMedium,
      MostVisitedCellSize(UIContentSizeCategoryExtraExtraLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kContentSuggestionsTileViewSizeMedium,
      MostVisitedCellSize(UIContentSizeCategoryExtraExtraExtraLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kContentSuggestionsTileViewSizeLarge,
      MostVisitedCellSize(UIContentSizeCategoryAccessibilityMedium)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kContentSuggestionsTileViewSizeExtraLarge,
      MostVisitedCellSize(UIContentSizeCategoryAccessibilityLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kContentSuggestionsTileViewSizeExtraLarge,
      MostVisitedCellSize(UIContentSizeCategoryAccessibilityExtraLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kContentSuggestionsTileViewSizeExtraLarge,
      MostVisitedCellSize(UIContentSizeCategoryAccessibilityExtraExtraLarge)));
  EXPECT_TRUE(CGSizeEqualToSize(
      kContentSuggestionsTileViewSizeExtraLarge,
      MostVisitedCellSize(
          UIContentSizeCategoryAccessibilityExtraExtraExtraLarge)));
}

// Tests that CenteredTilesMarginForWidth works under various environment.
TEST_F(ContentSuggestionsTileLayoutUtilTest, CenteredTilesMarginForWidth) {
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
                     kContentSuggestionsTileViewSizeSmall.width * 4 +
                         kContentSuggestionsTilesHorizontalSpacingRegular * 3 + 200 * 2));
  // Display 4 columns on normal screen.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kContentSuggestionsTileViewSizeSmall.width * 4 +
                        kContentSuggestionsTilesHorizontalSpacingRegular * 3 + 20 * 2));
  // Display 3 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kContentSuggestionsTileViewSizeSmall.width * 3 +
                        kContentSuggestionsTilesHorizontalSpacingRegular * 2 + 20 * 2));
  // Display 2 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kContentSuggestionsTileViewSizeSmall.width * 2 +
                        kContentSuggestionsTilesHorizontalSpacingRegular * 1 + 20 * 2));
  // Display 1 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kContentSuggestionsTileViewSizeSmall.width * 1 +
                        kContentSuggestionsTilesHorizontalSpacingRegular * 0 + 20 * 2));

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
                    kContentSuggestionsTileViewSizeExtraLarge.width * 4 +
                        kContentSuggestionsTilesHorizontalSpacingCompact * 3 + 20 * 2));
  // Display 3 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kContentSuggestionsTileViewSizeExtraLarge.width * 3 +
                        kContentSuggestionsTilesHorizontalSpacingCompact * 2 + 20 * 2));
  // Display 2 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kContentSuggestionsTileViewSizeExtraLarge.width * 2 +
                        kContentSuggestionsTilesHorizontalSpacingCompact * 1 + 20 * 2));
  // Display 1 columns.
  EXPECT_EQ(20, CenteredTilesMarginForWidth(
                    trait_collection,
                    kContentSuggestionsTileViewSizeExtraLarge.width * 1 +
                        kContentSuggestionsTilesHorizontalSpacingCompact * 0 + 20 * 2));
}
