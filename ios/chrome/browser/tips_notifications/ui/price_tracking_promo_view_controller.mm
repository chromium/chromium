// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/price_tracking_promo_view_controller.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The name of the file containing the price tracking promo animation.
NSString* const kAnimationName = @"price_tracking_promo";

// Returns the color provider for the animation.
NSDictionary<NSString*, UIColor*>* ColorProvider(int shadow_color) {
  return @{@"Shadow.*.*.Color" : UIColorFromRGB(shadow_color)};
}

// Accessibility identifier for the Tab Groups Promo view.
NSString* const kPriceTrackingPromoAccessibilityIdentifier =
    @"PriceTrackingPromoAccessbilityIdentifier";

}  // namespace

@implementation PriceTrackingPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.animationName = kAnimationName;
  self.lightModeColorProvider = ColorProvider(0xDADCE0);
  self.darkModeColorProvider = ColorProvider(0x5F6368);

  self.animationTextProvider = @{
    @"Price drop on your tracked product" : l10n_util::GetNSString(
        IDS_IOS_PRICE_TRACKING_PROMO_MOCK_NOTIFICATION_TITLE),
    @"Your item is now 15% off" : l10n_util::GetNSString(
        IDS_IOS_PRICE_TRACKING_PROMO_MOCK_NOTIFICATION_SUBTITLE),
  };

  self.titleText = l10n_util::GetNSString(IDS_IOS_PRICE_TRACKING_PROMO_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_PRICE_TRACKING_PROMO_SUBTITLE);

  self.configuration.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_PRICE_TRACKING_PROMO_TURN_ON_NOTIFICATIONS_BUTTON);
  self.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_PRICE_TRACKING_PROMO_KEEP_BROWSING_BUTTON);

  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kPriceTrackingPromoAccessibilityIdentifier;
}

@end
