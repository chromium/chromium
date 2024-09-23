// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

const int kContentSuggestionsTilesVerticalSpacing = 16;
const int kContentSuggestionsTilesHorizontalSpacingRegular = 19;
const int kContentSuggestionsTilesHorizontalSpacingCompact = 5;

const CGSize kContentSuggestionsTileViewSizeSmall = {/*width=*/73,
                                                     /*height=*/100};
const CGSize kContentSuggestionsTileViewSizeMedium = {/*width=*/73, /*height=*/112};
const CGSize kContentSuggestionsTileViewSizeLarge = {/*width=*/110, /*height=*/140};
const CGSize kContentSuggestionsTileViewSizeExtraLarge = {/*width=*/146, /*height=*/150};

namespace {
// Display at most 4 tiles per row.
const int kMaxNumberOfTilesPerRow = 4;
}

CGFloat ContentSuggestionsTilesHorizontalSpacing(UITraitCollection* trait_collection) {
  return (trait_collection.horizontalSizeClass !=
              UIUserInterfaceSizeClassCompact &&
          trait_collection.verticalSizeClass != UIUserInterfaceSizeClassCompact)
             ? kContentSuggestionsTilesHorizontalSpacingRegular
             : kContentSuggestionsTilesHorizontalSpacingCompact;
}

CGSize MostVisitedCellSize(UIContentSizeCategory category) {
  NSComparisonResult result = UIContentSizeCategoryCompareToCategory(
      category, UIContentSizeCategoryAccessibilityMedium);
  BOOL isSmallestSize =
      UIContentSizeCategoryCompareToCategory(
          category, UIContentSizeCategoryExtraLarge) == NSOrderedAscending;
  switch (result) {
    case NSOrderedAscending:
      return isSmallestSize ? kContentSuggestionsTileViewSizeSmall
                            : kContentSuggestionsTileViewSizeMedium;
    case NSOrderedSame:
      return kContentSuggestionsTileViewSizeLarge;
    case NSOrderedDescending:
      return kContentSuggestionsTileViewSizeExtraLarge;
  }
}

CGFloat CenteredTilesMarginForWidth(UITraitCollection* trait_collection,
                                    CGFloat width) {
  CGFloat horizontalSpace = ContentSuggestionsTilesHorizontalSpacing(trait_collection);
  CGSize cellSize =
      MostVisitedCellSize(trait_collection.preferredContentSizeCategory);
  for (int columns = kMaxNumberOfTilesPerRow; columns > 0; --columns) {
    CGFloat whitespace =
        width - (columns * cellSize.width) - ((columns - 1) * horizontalSpace);
    CGFloat margin = AlignValueToPixel(whitespace / 2);
    if (margin >= horizontalSpace) {
      return margin;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

CGFloat MostVisitedTilesContentHorizontalSpace(
    UITraitCollection* trait_collection) {
  CGFloat horizontalSpace =
      ContentSuggestionsTilesHorizontalSpacing(trait_collection);
  CGSize cellSize =
      MostVisitedCellSize(trait_collection.preferredContentSizeCategory);
  // Sum up the space taken up by all the tiles and space between them.
  CGFloat width = (kMaxNumberOfTilesPerRow * cellSize.width) +
                  ((kMaxNumberOfTilesPerRow - 1) * horizontalSpace);
  return width;
}
