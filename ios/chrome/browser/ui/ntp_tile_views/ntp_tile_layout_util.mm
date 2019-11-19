// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_layout_util.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const int kNtpTilesVerticalSpacing = 16;
const int kNtpTilesHorizontalSpacingRegular = 19;
const int kNtpTilesHorizontalSpacingCompact = 5;

const CGSize kNtpTileViewSizeSmall = {/*width=*/73, /*height=*/100};
const CGSize kNtpTileViewSizeMedium = {/*width=*/73, /*height=*/112};
const CGSize kNtpTileViewSizeLarge = {/*width=*/110, /*height=*/140};
const CGSize kNtpTileViewSizeExtraLarge = {/*width=*/146, /*height=*/150};

namespace {
// Display at most 4 tiles per row.
const int kMaxNumberOfTilesPerRow = 4;
}

CGFloat NtpTilesHorizontalSpacing(UITraitCollection* trait_collection) {
  return (trait_collection.horizontalSizeClass !=
              UIUserInterfaceSizeClassCompact &&
          trait_collection.verticalSizeClass != UIUserInterfaceSizeClassCompact)
             ? kNtpTilesHorizontalSpacingRegular
             : kNtpTilesHorizontalSpacingCompact;
}

CGSize MostVisitedCellSize(UIContentSizeCategory category) {
  NSComparisonResult result = UIContentSizeCategoryCompareToCategory(
      category, UIContentSizeCategoryAccessibilityMedium);
  switch (result) {
    case NSOrderedAscending:
      return ([category
                 isEqualToString:UIContentSizeCategoryExtraExtraExtraLarge])
                 ? kNtpTileViewSizeMedium
                 : kNtpTileViewSizeSmall;
    case NSOrderedSame:
      return kNtpTileViewSizeLarge;
    case NSOrderedDescending:
      return kNtpTileViewSizeExtraLarge;
  }
}

CGFloat CenteredTilesMarginForWidth(UITraitCollection* trait_collection,
                                    CGFloat width) {
  CGFloat horizontalSpace = NtpTilesHorizontalSpacing(trait_collection);
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
  NOTREACHED();
  return 0;
}
