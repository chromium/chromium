// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/badges/sc_badge_coordinator.h"

#import "ios/chrome/browser/ui/badges/badge_button_factory.h"
#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/badges/badge_delegate.h"
#import "ios/chrome/browser/ui/badges/badge_popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/badges/badge_tappable_item.h"
#import "ios/chrome/browser/ui/badges/badge_type.h"
#import "ios/chrome/browser/ui/badges/badge_view_controller.h"
#import "ios/showcase/badges/sc_badge_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCBadgeCoordinator () <BadgeDelegate>
@property(nonatomic, strong)
    SCBadgeContainerViewController* containerViewController;
@property(nonatomic, weak, readonly) id<BadgeConsumer> consumer;
@property(nonatomic, strong)
    BadgePopupMenuCoordinator* badgePopupMenuCoordinator;
@end

@implementation SCBadgeCoordinator
@synthesize baseViewController = _baseViewController;

- (NSArray<NSNumber*>*)badgeTypesForOverflowMenu {
  return @[ @(kBadgeTypePasswordSave) ];
}

- (void)start {
  self.containerViewController =
      [[SCBadgeContainerViewController alloc] initWithBadgeDelegate:self];
  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
}

- (id<BadgeConsumer>)consumer {
  return self.containerViewController.consumer;
}

- (void)addToReadingListBadgeButtonTapped:(id)sender {
}

- (void)passwordsBadgeButtonTapped:(id)sender {
}

- (void)saveCardBadgeButtonTapped:(id)sender {
}

- (void)saveAddressProfileBadgeButtonTapped:(id)sender {
}

- (void)translateBadgeButtonTapped:(id)sender {
}

- (void)permissionsBadgeButtonTapped:(id)sender {
}

- (void)overflowBadgeButtonTapped:(id)sender {
  if (!self.containerViewController.useNewPopupUI) {
    self.badgePopupMenuCoordinator = [[BadgePopupMenuCoordinator alloc]
        initWithBaseViewController:self.containerViewController
                           browser:nil];
    NSArray* badgeItems = @[ [[BadgeTappableItem alloc]
        initWithBadgeType:kBadgeTypePasswordSave] ];
    [self.badgePopupMenuCoordinator setBadgeItemsToShow:badgeItems];
    [self.badgePopupMenuCoordinator start];
  }
  [self.consumer markDisplayedBadgeAsRead:YES];
}

- (void)showModalForBadgeType:(BadgeType)badgeType {
}

@end
