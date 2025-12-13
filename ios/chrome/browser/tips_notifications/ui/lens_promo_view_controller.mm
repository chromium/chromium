// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/lens_promo_view_controller.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The names of the files containing the lens promo animation.
NSString* const kAnimationName = @"lens_promo";

// Accessibility identifier for the Lens Promo view.
NSString* const kLensPromoAXID = @"kLensPromoAXID";

// Returns the color provider for the animation.
NSDictionary<NSString*, UIColor*>* ColorProvider(int omnibox_color,
                                                 int lens_background_color) {
  return @{
    @"Omnibox.*.*.Color" : UIColorFromRGB(omnibox_color),
    @"Lens_Icon_Background.*.*.Color" : UIColorFromRGB(lens_background_color),
  };
}

}  // namespace

@implementation LensPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.animationName = kAnimationName;

  self.lightModeColorProvider = ColorProvider(0xEDF4FE, 0xFFFFFF);
  self.darkModeColorProvider = ColorProvider(0x232428, 0x464A4E);

  self.titleText = l10n_util::GetNSString(IDS_IOS_LENS_PROMO_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_LENS_PROMO_SUBTITLE);

  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_LENS_PROMO_PRIMARY_ACTION);
  self.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_LENS_PROMO_SHOW_ME_HOW);
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kLensPromoAXID;
}

@end
