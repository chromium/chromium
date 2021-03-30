// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/location_permissions_view_controller.h"

#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation LocationPermissionsViewController

#pragma mark - Public

- (void)loadView {
  self.image = [UIImage imageNamed:@"first_run_location_permissions"];
  self.customSpacingAfterImage = 30;
  self.primaryActionAvailable = YES;
  self.secondaryActionAvailable = YES;
  self.showDismissBarButton = NO;
  self.titleString = l10n_util::GetNSString(IDS_IOS_LOCATION_MODAL_TITLE);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_LOCATION_MODAL_DESCRIPTION);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_LOCATION_MODAL_PRIMARY_BUTTON);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_SECONDARY_BUTTON_TEXT);
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemCancel;
  if (@available(iOS 13.4, *)) {
    self.pointerInteractionEnabled = YES;
  }
  [super loadView];
}

@end
