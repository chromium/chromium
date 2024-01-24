// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_history_sync_view_controller.h"

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_constants.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* const kHistorySyncBannerName = @"history_sync_illustration";

}  // namespace

@implementation PrivacyGuideHistorySyncViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kPrivacyGuideHistorySyncViewID;

  self.bannerName = kHistorySyncBannerName;
  self.bannerSize = BannerImageSizeType::kShort;

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PRIVACY_GUIDE_NEXT_BUTTON);

  self.subtitleBottomMargin = 0;

  [super viewDidLoad];
}

@end
