// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/content_notification/sc_content_notification_fullscreen_promo_coordinator.h"

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_view_controller.h"

@interface SCContentNotificationFullscreenPromoCoordinator ()

@property(nonatomic, strong)
    SetUpListContentNotificationPromoViewController* viewController;

@end

@implementation SCContentNotificationFullscreenPromoCoordinator
@synthesize baseViewController = _baseViewController;

#pragma mark - Public Methods.

- (void)start {
  self.viewController =
      [[SetUpListContentNotificationPromoViewController alloc] init];
  self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  [self.baseViewController setHidesBarsOnSwipe:NO];
  [self.baseViewController pushViewController:self.viewController animated:YES];
}

@end
