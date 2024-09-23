// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/enhanced_safe_browsing_promo_view_controller.h"

#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The names of the files containing the Enhanced Safe Browsing promo animation.
NSString* const kAnimationName = @"enhanced_safe_browsing_promo";
NSString* const kAnimationNameDarkMode =
    @"enhanced_safe_browsing_promo_darkmode";

// Accessibility identifier for the Enhanced Safe Browsing Promo view.
NSString* const kEnhancedSafeBrowsingPromoAXID =
    @"kEnhancedSafeBrowsingPromoAXID";

}  // namespace

@implementation EnhancedSafeBrowsingPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.animationName = kAnimationName;
  self.animationNameDarkMode = kAnimationNameDarkMode;
  self.animationTextProvider = @{
    @"Safe Browsing" :
        l10n_util::GetNSString(IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE),
    @"Enhanced Protection" : l10n_util::GetNSString(
        IDS_IOS_PRIVACY_SAFE_BROWSING_ENHANCED_PROTECTION_TITLE),
    @"Standard Protection" : l10n_util::GetNSString(
        IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_TITLE),
    @"No Protection" : l10n_util::GetNSString(
        IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_DETAIL_TITLE),
  };

  self.titleText =
      l10n_util::GetNSString(IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_SUBTITLE);

  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_PRIMARY_ACTION);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_SHOW_ME_HOW);
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kEnhancedSafeBrowsingPromoAXID;
}

@end
