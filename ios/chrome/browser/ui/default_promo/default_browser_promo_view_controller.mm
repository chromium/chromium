// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_view_controller.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_string_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation DefaultBrowserPromoViewController

#pragma mark - Public

- (void)loadView {
  self.image = [UIImage imageNamed:@"default_browser_illustration"];
  self.customSpacingAfterImage = 30;

  self.helpButtonAvailable = YES;
  self.helpButtonAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HELP_ACCESSIBILITY_LABEL);

  self.showDismissBarButton = NO;
  self.titleString = GetDefaultBrowserPromoTitle();
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_DESCRIPTION);
  self.primaryActionString = l10n_util::GetNSString(IDS_IOS_OPEN_SETTINGS);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_SECONDARY_BUTTON_TEXT);
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemCancel;
  [super loadView];
}

@end
