// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_view_controller.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* kURLUsageBannerName = @"url_usage_illustration";

}  // namespace

@implementation PrivacyGuideURLUsageViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kPrivacyGuideURLUsageViewID;

  self.bannerName = kURLUsageBannerName;
  self.bannerSize = BannerImageSizeType::kShort;

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PRIVACY_GUIDE_NEXT_BUTTON);

  self.subtitleBottomMargin = 0;

  [super viewDidLoad];
}

@end
