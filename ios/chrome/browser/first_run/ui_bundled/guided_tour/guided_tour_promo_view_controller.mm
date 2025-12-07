// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/guided_tour/guided_tour_promo_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Spacing above the title.
const CGFloat kTitleTopMarginWhenNoHeaderImage = 24;
}  // namespace

@implementation GuidedTourPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.bannerSize = BannerImageSizeType::kTall;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  self.bannerName = kChromeGuidedTourBannerImage;
#else
  self.bannerName = kChromiumGuidedTourBannerImage;
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

  self.shouldBannerFillTopSpace = YES;
  self.titleTopMarginWhenNoHeaderImage = kTitleTopMarginWhenNoHeaderImage;
  self.titleText =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_GUIDED_TOUR_PROMO_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_GUIDED_TOUR_PROMO_TEXT);
  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_GUIDED_TOUR_PROMPT_BUTTON_TITLE);
  self.configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION);
  [super viewDidLoad];
}

@end
