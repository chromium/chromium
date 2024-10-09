// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/device_util.h"

const CGFloat kTableViewHeaderFooterViewHeight = 48.0;
const CGFloat kChromeTableViewCellHeight = 48.0;
const CGFloat kTableViewHorizontalSpacing = 16.0;
const CGFloat kTableViewOneLabelCellVerticalSpacing = 14.0;
const CGFloat kTableViewTwoLabelsCellVerticalSpacing = 11.0;
const CGFloat kTableViewVerticalSpacing = 8.0;
const CGFloat kTableViewLargeVerticalSpacing = 16.0;
const CGFloat kTableViewSubViewHorizontalSpacing = 12.0;
const CGFloat kTableViewCellSelectionAnimationDuration = 0.15;
const CGFloat kUseDefaultFontSize = 0.0;
const CGFloat kTableViewLabelVerticalTopSpacing = 13.0;
const CGFloat kTableViewAccessoryWidth = 40;
const CGFloat kTableViewIconImageSize = 30;
const CGFloat kTableViewImagePadding = 14;
const CGFloat kTableViewTrailingContentPadding = 6;

NSString* const kMaskedPassword = @"••••••••";
NSString* const kTableViewCellInfoButtonViewId =
    @"kTableViewCellInfoButtonViewId";
NSString* const kTableViewTabsSearchSuggestedHistoryItemId =
    @"kTableViewTabsSearchSuggestedHistoryItemId";

NSString* const kTableViewURLCellFaviconBadgeViewID =
    @"TableViewURLCellFaviconBadgeView";

NSString* const kTableViewURLCellMetadataImageID =
    @"TableViewURLCellMetadataImageID";

NSString* const kImproveChromeItemAccessibilityIdentifier =
    @"ImproveChromeItemAccessibilityIdentifier";

NSString* const kTableViewActivityIndicatorHeaderFooterViewId =
    @"TableViewActivityIndicatorHeaderFooterViewId";

CGFloat HorizontalPadding() {
  if (!IsSmallDevice())
    return 0;
  return kTableViewHorizontalSpacing;
}
