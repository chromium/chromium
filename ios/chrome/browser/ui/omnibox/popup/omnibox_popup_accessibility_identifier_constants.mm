// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kOmniboxPopupRowSwitchTabAccessibilityIdentifier =
    @"OmniboxPopupRowSwitchTabAccessibilityIdentifier";

NSString* const kOmniboxPopupRowAppendAccessibilityIdentifier =
    @"OmniboxPopupRowAppendAccessibilityIdentifier";

NSString* const kOmniboxPopupTableViewAccessibilityIdentifier =
    @"OmniboxPopupTableViewAccessibilityIdentifier";

NSString* const kOmniboxCarouselCellAccessibilityIdentifier =
    @"OmniboxCarouselCellAccessibilityIdentifier";

NSString* const kOmniboxCarouselControlLabelAccessibilityIdentifier =
    @"OmniboxCarouselControlLabelAccessibilityIdentifier";

@implementation OmniboxPopupAccessibilityIdentifierHelper

+ (NSString*)accessibilityIdentifierForRowAtIndexPath:(NSIndexPath*)indexPath {
  return
      [NSString stringWithFormat:@"omnibox suggestion %ld %ld",
                                 (long)indexPath.section, (long)indexPath.row];
}

@end
