// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_signout/enterprise_signout_view_controller.h"

#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation EnterpriseSignoutViewController

#pragma mark - Public

- (void)loadView {
  self.image = [UIImage imageNamed:@"enterprise_grey_icon_large"];
  self.imageHasFixedSize = YES;
  self.customSpacingAfterImage = 30;

  self.showDismissBarButton = NO;
  self.titleString = l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT);
  self.subtitleString = l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_RESTRICTED_ACCOUNTS_TO_PATTERNS_MESSAGE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT_CONTINUE);
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemDone;

  if (@available(iOS 15, *)) {
    self.titleTextStyle = UIFontTextStyleTitle2;
    // Icon already contains some spacing for the shadow.
    self.customSpacingBeforeImageIfNoToolbar = 24;
    self.customSpacingAfterImage = 1;
    self.topAlignedLayout = YES;
  }

  [super loadView];
}

@end
