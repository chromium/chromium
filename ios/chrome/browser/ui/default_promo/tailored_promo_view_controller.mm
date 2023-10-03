// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/tailored_promo_view_controller.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_string_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation TailoredPromoViewController

#pragma mark - Public

- (void)loadView {
  self.customSpacingAfterImage = 30;
  self.helpButtonAvailable = YES;
  self.helpButtonAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HELP_ACCESSIBILITY_LABEL);
  self.imageHasFixedSize = YES;
  self.showDismissBarButton = NO;
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemCancel;
  [super loadView];
}

@end
