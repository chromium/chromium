// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/public/omnibox_popup_accessibility_identifier_constants.h"

#import <UIKit/UIKit.h>

NSString* const kOmniboxPopupRowSwitchTabAccessibilityIdentifier =
    @"OmniboxPopupRowSwitchTabAccessibilityIdentifier";

NSString* const kOmniboxPopupRowAppendAccessibilityIdentifier =
    @"OmniboxPopupRowAppendAccessibilityIdentifier";

NSString* const kOmniboxPopupRowPrimaryTextAccessibilityIdentifier =
    @"OmniboxPopupRowPrimaryTextAccessibilityIdentifier";

NSString* const kOmniboxPopupRowSecondaryTextAccessibilityIdentifier =
    @"OmniboxPopupRowSecondaryTextAccessibilityIdentifier";

NSString* const kOmniboxPopupTableViewAccessibilityIdentifier =
    @"OmniboxPopupTableViewAccessibilityIdentifier";

NSString* const kOmniboxCarouselCellAccessibilityIdentifier =
    @"OmniboxCarouselCellAccessibilityIdentifier";

NSString* const kOmniboxCarouselControlLabelAccessibilityIdentifier =
    @"OmniboxCarouselControlLabelAccessibilityIdentifier";

NSString* const kDirectionsActionHighlightedIdentifier =
    @"kDirectionsActionHighlightedIdentifier";

NSString* const kDirectionsActionIdentifier = @"kDirectionsActionIdentifier";

NSString* const kCallActionHighlightedIdentifier =
    @"kCallActionHighlightedIdentifier";

NSString* const kCallActionIdentifier = @"kCallActionIdentifier";

NSString* const kReviewsActionHighlightedIdentifier =
    @"kReviewsActionHighlightedIdentifier";

NSString* const kReviewsActionIdentifier = @"kReviewsActionIdentifier";

NSString* const kOmniboxPopupRowSearchWithAimAccessibilityIdentifier =
    @"OmniboxPopupRowSearchWithAimAccessibilityIdentifier";

@implementation OmniboxPopupAccessibilityIdentifierHelper

+ (NSString*)accessibilityIdentifierForRowAtIndexPath:(NSIndexPath*)indexPath {
  return
      [NSString stringWithFormat:@"omnibox suggestion %ld %ld",
                                 (long)indexPath.section, (long)indexPath.row];
}

@end
