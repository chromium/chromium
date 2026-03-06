// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/tab_groups_promo_view_controller.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The names of the files containing the lens promo animation.
NSString* const kAnimationName = @"tab_groups_promo";

// Returns the color provider for the animation.
NSDictionary<NSString*, UIColor*>* ColorProvider(int shadow_color) {
  return @{@"Shadow.*.*.Color" : UIColorFromRGB(shadow_color)};
}

// Accessibility identifier for the Tab Groups Promo view.
NSString* const kTabGroupsPromoAccessibilityIdentifier =
    @"TabGroupsPromoAccessbilityIdentifier";
}  // namespace

@implementation TabGroupsPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.animationName = kAnimationName;
  self.lightModeColorProvider = ColorProvider(0xDADCE0);
  self.darkModeColorProvider = ColorProvider(0x5F6368);

  self.titleText = l10n_util::GetNSString(IDS_IOS_TAB_GROUPS_PROMO_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_TAB_GROUPS_PROMO_SUBTITLE);

  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_TAB_GROUPS_PROMO_TAKE_ME_THERE_BUTTON);
  self.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_TAB_GROUPS_PROMO_KEEP_BROWSING_BUTTON);

  [super viewDidLoad];
  self.view.accessibilityIdentifier = kTabGroupsPromoAccessibilityIdentifier;
}

@end
