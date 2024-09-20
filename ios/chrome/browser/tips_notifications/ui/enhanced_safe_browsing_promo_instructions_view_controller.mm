// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/enhanced_safe_browsing_promo_instructions_view_controller.h"

#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// AccessibilityIdentifier for the Enhanced Safe Browsing Promo Instructions
// view.
NSString* const kEnhancedSafeBrowsingPromoInstructionsAXID =
    @"kEnhancedSafeBrowsingPromoInstructionsAXID";

}  // namespace

@implementation EnhancedSafeBrowsingPromoInstructionsViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.titleString = l10n_util::GetNSString(
      IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_TITLE);
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_PRIMARY_ACTION);
  self.steps = @[
    l10n_util::GetNSString(
        IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_STEP1),
    l10n_util::GetNSString(
        IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_STEP2),
    l10n_util::GetNSString(
        IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_STEP3),
  ];

  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kEnhancedSafeBrowsingPromoInstructionsAXID;
}

@end
