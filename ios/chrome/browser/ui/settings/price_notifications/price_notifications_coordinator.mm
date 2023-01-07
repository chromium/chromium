// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_coordinator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_mediator.h"
#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_view_controller.h"
#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_coordinator.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PriceNotificationsCoordinator () <
    PriceNotificationsNavigationCommands,
    PriceNotificationsViewControllerPresentationDelegate,
    TrackingPriceCoordinatorDelegate>

// View controller presented by coordinator.
@property(nonatomic, strong) PriceNotificationsViewController* viewController;
// Price notifications settings mediator.
@property(nonatomic, strong) PriceNotificationsMediator* mediator;
// Coordinator for Tracking Price settings menu.
@property(nonatomic, strong) TrackingPriceCoordinator* trackingPriceCoordinator;

@end

@implementation PriceNotificationsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ([super initWithBaseViewController:navigationController browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  self.viewController = [[PriceNotificationsViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.presentationDelegate = self;
  self.mediator = [[PriceNotificationsMediator alloc] init];
  self.mediator.consumer = self.viewController;
  self.mediator.handler = self;
  self.viewController.modelDelegate = self.mediator;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

#pragma mark - PriceNotificationsNavigationCommands

- (void)showTrackingPrice {
  DCHECK(!self.trackingPriceCoordinator);
  DCHECK(self.baseNavigationController);
  self.trackingPriceCoordinator = [[TrackingPriceCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.trackingPriceCoordinator.delegate = self;
  [self.trackingPriceCoordinator start];
}

#pragma mark - PriceNotificationsViewControllerPresentationDelegate

- (void)priceNotificationsViewControllerDidRemove:
    (PriceNotificationsViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate priceNotificationsCoordinatorDidRemove:self];
}

#pragma mark - TrackingPriceCoordinatorDelegate

- (void)trackingPriceCoordinatorDidRemove:
    (TrackingPriceCoordinator*)coordinator {
  DCHECK_EQ(self.trackingPriceCoordinator, coordinator);
  [self.trackingPriceCoordinator stop];
  self.trackingPriceCoordinator.delegate = nil;
  self.trackingPriceCoordinator = nil;
}

@end
