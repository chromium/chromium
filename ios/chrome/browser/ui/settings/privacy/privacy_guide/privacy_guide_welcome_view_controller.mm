// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_view_controller.h"

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_constants.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr NSString* kWelcomeBannerName = @"welcome_illustration";

}  // namespace

@implementation PrivacyGuideWelcomeViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kPrivacyGuideWelcomeViewID;

  self.bannerName = kWelcomeBannerName;
  self.bannerSize = BannerImageSizeType::kTall;

  self.titleText = l10n_util::GetNSString(IDS_IOS_PRIVACY_GUIDE_WELCOME_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_PRIVACY_GUIDE_WELCOME_SUBTITLE);

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PRIVACY_GUIDE_LETS_GO_BUTTON);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_PRIVACY_GUIDE_CANCEL_BUTTON);

  [super viewDidLoad];
}

@end
