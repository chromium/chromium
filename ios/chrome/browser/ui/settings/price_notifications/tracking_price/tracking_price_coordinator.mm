// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_coordinator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_mediator.h"
#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

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

@implementation TrackingPriceCoordinator {
  // Coordinator for displaying alerts.
  AlertCoordinator* _alertCoordinator;
}

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
  self.mediator.presenter = self;
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

#pragma mark - PriceNotificationsAlertPresenter

- (void)presentPushNotificationPermissionAlert {
  NSString* settingURL = UIApplicationOpenSettingsURLString;
  if (@available(iOS 15.4, *)) {
    settingURL = UIApplicationOpenNotificationSettingsURLString;
  }

  NSString* alertTitle =
      l10n_util::GetNSString(IDS_IOS_PRICE_NOTIFICATIONS_SETTINGS_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_SETTINGS_ALERT_MESSAGE);
  NSString* cancelTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_CANCEL);
  NSString* settingsTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_REDIRECT);

  [_alertCoordinator stop];
  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:alertTitle
                                                   message:alertMessage];
  [_alertCoordinator addItemWithTitle:cancelTitle
                               action:nil
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator
      addItemWithTitle:settingsTitle
                action:^{
                  [[UIApplication sharedApplication]
                                openURL:[NSURL URLWithString:settingURL]
                                options:{}
                      completionHandler:nil];
                }
                 style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

@end
