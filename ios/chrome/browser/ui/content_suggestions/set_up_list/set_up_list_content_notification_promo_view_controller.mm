// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_view_controller.h"

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"

@implementation SetUpListContentNotificationPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = set_up_list::kContentNotificationItemID;
  self.bannerName = @"content_notification_promo_banner";
  // TODO(b/310713830): Update the strings when finalized.
  self.titleText = @"Get content that matters to you";
  self.subtitleText = @"Receive notificarions for news, sports and more based "
                      @"on your interests";
  self.primaryActionString = @"Turn on Notifications..";
  self.secondaryActionString = @"No Thanks";
  self.tertiaryActionString = @"Remind Me Later";

  [super viewDidLoad];
}

@end
