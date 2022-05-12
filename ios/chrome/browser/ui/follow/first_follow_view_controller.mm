// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/first_follow_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/follow/first_follow_favicon_data_source.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr CGFloat customSpacingBeforeImageIfNoToolbar = 24;
constexpr CGFloat customSpacingAfterImage = 1;

}  // namespace

@implementation FirstFollowViewController

- (void)viewDidLoad {
  self.imageHasFixedSize = YES;
  self.showDismissBarButton = NO;
  self.customSpacingBeforeImageIfNoToolbar =
      customSpacingBeforeImageIfNoToolbar;
  self.customSpacingAfterImage = customSpacingAfterImage;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;

  self.titleString = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_FOLLOW_TITLE,
      base::SysNSStringToUTF16(self.followedWebChannel.title));
  self.secondaryTitleString = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_FOLLOW_SUBTITLE,
      base::SysNSStringToUTF16(self.followedWebChannel.title));
  self.subtitleString = l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_BODY);

  if (self.followedWebChannel.available) {
    // Go To Feed button is only displayed if the web channel is available.
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_GO_TO_FEED);
    self.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_GOT_IT);
  } else {
    // Only one button is visible, and it is a primary action button (with a
    // solid background color).
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_GOT_IT);
  }

  // TODO(crbug.com/1312124): Favicon styling needs more whitespace, shadow, and
  // corner green checkmark badge.
  if (self.followedWebChannel.webPageURL) {
    [self.faviconDataSource faviconForURL:self.followedWebChannel.webPageURL
                               completion:^(FaviconAttributes* attributes) {
                                 self.image = attributes.faviconImage;
                               }];
  }

  [super viewDidLoad];
}

@end
