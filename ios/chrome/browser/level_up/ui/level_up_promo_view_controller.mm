// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_promo_view_controller.h"

#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation LevelUpPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  // TODO: add dismiss button on nav bar
  // TODO: add banner image
  self.shouldHideBanner = YES;
  self.titleText = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_SUBTITLE);
  self.titleHorizontalMargin = 0;

  // TODO: add checklist content
  // TODO: add disclaimer text
  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_PRIMARY_BUTTON);
  self.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_SECONDARY_BUTTON);
  [super viewDidLoad];
}

@end
