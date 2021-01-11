// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/default_browser_promo_view_controller.h"

#include "base/feature_list.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation DefaultBrowserPromoViewController

#pragma mark - Public

- (void)loadView {
  self.image = [UIImage imageNamed:@"default_browser_illustration"];
  self.customSpacingAfterImage = 30;

  self.helpButtonAvailable = YES;

  self.primaryActionAvailable = YES;
  self.secondaryActionAvailable = YES;
  self.showDismissBarButton = NO;
  self.titleString = l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_TITLE);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_DESCRIPTION);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_MAIN_BUTTON_TEXT);
  if (base::FeatureList::IsEnabled(kDefaultBrowserFullscreenPromoExperiment)) {
    // TODO:(crubg.com/1155778): Add translation string.
    self.secondaryActionString = @"Remind Me Later";
    self.tertiaryActionAvailable = YES;
    self.tertiaryActionString =
        l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_SECONDARY_BUTTON_TEXT);
  } else {
    self.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_SECONDARY_BUTTON_TEXT);
  }
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemCancel;

#if defined(__IPHONE_13_4)
  if (@available(iOS 13.4, *)) {
    self.pointerInteractionEnabled = YES;
  }
#endif  // defined(__IPHONE_13_4)
  [super loadView];
}

@end
