// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/lens_promo_view_controller.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The names of the files containing the lens promo animation.
NSString* const kAnimationName = @"lens_promo";
NSString* const kAnimationNameDarkMode = @"lens_promo_darkmode";

// Accessibility identifier for the Lens Promo view.
NSString* const kLensPromoAXID = @"kLensPromoAXID";

}  // namespace

@implementation LensPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.animationName = kAnimationName;
  self.animationNameDarkMode = kAnimationNameDarkMode;

  self.titleText = l10n_util::GetNSString(IDS_IOS_LENS_PROMO_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_LENS_PROMO_SUBTITLE);

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_LENS_PROMO_PRIMARY_ACTION);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_LENS_PROMO_SHOW_ME_HOW);
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kLensPromoAXID;
}

@end
