// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_coordinator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_mediator.h"
#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TrackingPriceCoordinator () <
    TrackingPriceViewControllerPresentationDelegate>

// View controller presented by coordinator.
@property(nonatomic, strong) TrackingPriceViewController* viewController;
// Tracking Price settings mediator.
@property(nonatomic, strong) TrackingPriceMediator* mediator;

@end

@implementation TrackingPriceCoordinator

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
  self.viewController = [[TrackingPriceViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.presentationDelegate = self;
  self.mediator = [[TrackingPriceMediator alloc]
      initWithBrowserState:self.browser->GetBrowserState()];
  self.mediator.consumer = self.viewController;
  self.viewController.modelDelegate = self.mediator;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

#pragma mark - TrackingPriceViewControllerPresentationDelegate

- (void)trackingPriceViewControllerDidRemove:
    (TrackingPriceViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate trackingPriceCoordinatorDidRemove:self];
}

@end
