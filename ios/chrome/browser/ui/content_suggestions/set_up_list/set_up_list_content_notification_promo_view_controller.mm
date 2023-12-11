// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_view_controller.h"

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation SetUpListContentNotificationPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = set_up_list::kContentNotificationItemID;
  self.bannerName = @"content_notification_promo_banner";
  self.titleText = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATION_FULL_PAGE_PROMO_TITLE);
  self.subtitleText = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATION_FULL_PAGE_PROMO_SUBTITLE);
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATION_FULL_PAGE_PROMO_PRIMARY_BUTTON_TEXT);
  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATION_FULL_PAGE_PROMO_SECONDARY_BUTTON_TEXT);
  self.tertiaryActionString = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATION_FULL_PAGE_PROMO_TERTIARY_BUTTON_TEXT);

  [super viewDidLoad];
}

@end
