// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/badges/sc_badge_coordinator.h"

#import "ios/chrome/browser/ui/badges/badge_button_factory.h"
#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/badges/badge_delegate.h"
#import "ios/chrome/browser/ui/badges/badge_tappable_item.h"
#import "ios/chrome/browser/ui/badges/badge_type.h"
#import "ios/chrome/browser/ui/badges/badge_view_controller.h"
#import "ios/showcase/badges/sc_badge_container_view_controller.h"

@interface SCBadgeCoordinator () <BadgeDelegate>
@property(nonatomic, strong)
    SCBadgeContainerViewController* containerViewController;
@property(nonatomic, weak, readonly) id<BadgeConsumer> consumer;
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
  [self.consumer markDisplayedBadgeAsRead:YES];
}

- (void)showModalForBadgeType:(BadgeType)badgeType {
}

@end
