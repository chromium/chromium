// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_earl_grey_ui_test_util.h"

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@implementation InfobarEarlGreyUI

+ (void)waitUntilInfobarBannerVisibleOrTimeout:(BOOL)shouldShow {
  GREYCondition* infobar_shown = [GREYCondition
      conditionWithName:@"Infobar shown"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey
                        selectElementWithMatcher:
                            grey_accessibilityID(kInfobarBannerViewIdentifier)]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  // Wait for infobar to be shown or timeout after kWaitForUIElementTimeout.
  BOOL success = [infobar_shown
      waitWithTimeout:base::test::ios::kWaitForUIElementTimeout.InSecondsF()];
  if (shouldShow) {
    GREYAssertTrue(success, @"Infobar does not appear.");
  } else {
    GREYAssertFalse(success, @"Infobar appeared.");
  }
}

@end
