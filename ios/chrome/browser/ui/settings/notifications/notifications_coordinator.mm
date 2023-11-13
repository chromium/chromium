// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/notifications_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_mediator.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_view_controller.h"
#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_coordinator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface NotificationsCoordinator () <
    NotificationsNavigationCommands,
    NotificationsViewControllerPresentationDelegate,
    TrackingPriceCoordinatorDelegate>

// View controller presented by coordinator.
@property(nonatomic, strong) NotificationsViewController* viewController;
// Notifications settings mediator.
@property(nonatomic, strong) NotificationsMediator* mediator;
// Coordinator for Tracking Price settings menu.
@property(nonatomic, strong) TrackingPriceCoordinator* trackingPriceCoordinator;
// An observer that tracks whether push notification permission settings have
// been modified.
@property(nonatomic, strong)
    NotificationsSettingsObserver* notificationsObserver;
// Alert Coordinator used to display the notifications system prompt.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

@end

@implementation NotificationsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  const std::string& gaiaID = base::SysNSStringToUTF8(identity.gaiaID);
  PrefService* prefService = self.browser->GetBrowserState()->GetPrefs();
  _notificationsObserver =
      [[NotificationsSettingsObserver alloc] initWithPrefService:prefService];

  self.viewController = [[NotificationsViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.presentationDelegate = self;
  self.mediator = [[NotificationsMediator alloc] initWithPrefService:prefService
                                                              gaiaID:gaiaID];
  self.mediator.consumer = self.viewController;
  self.mediator.handler = self;
  _notificationsObserver.delegate = self.mediator;
  self.viewController.modelDelegate = self.mediator;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

#pragma mark - NotificationsAlertPresenter

- (void)presentPushNotificationPermissionAlert {
  NSString* settingURL = UIApplicationOpenSettingsURLString;
  if (@available(iOS 15.4, *)) {
    settingURL = UIApplicationOpenNotificationSettingsURLString;
  }
  // TODO(b/304781544): Update the alert strings.
  NSString* alertTitle =
      l10n_util::GetNSString(IDS_IOS_PRICE_NOTIFICATIONS_SETTINGS_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_SETTINGS_ALERT_MESSAGE);
  NSString* cancelTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_CANCEL);
  NSString* settingsTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_REDIRECT);

  __weak NotificationsCoordinator* weakSelf = self;
  [self.alertCoordinator stop];
  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:alertTitle
                                                   message:alertMessage];
  [self.alertCoordinator addItemWithTitle:cancelTitle
                                   action:^{
                                     [weakSelf dimissAlertCoordinator];
                                   }
                                    style:UIAlertActionStyleCancel];
  [self.alertCoordinator
      addItemWithTitle:settingsTitle
                action:^{
                  [[UIApplication sharedApplication]
                                openURL:[NSURL URLWithString:settingURL]
                                options:{}
                      completionHandler:nil];
                  [weakSelf dimissAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault];
  [self.alertCoordinator start];
}

#pragma mark - Private

- (void)dimissAlertCoordinator {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

#pragma mark - NotificationsNavigationCommands

- (void)showTrackingPrice {
  DCHECK(!self.trackingPriceCoordinator);
  DCHECK(self.baseNavigationController);
  self.trackingPriceCoordinator = [[TrackingPriceCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.trackingPriceCoordinator.delegate = self;
  [self.trackingPriceCoordinator start];
}

#pragma mark - NotificationsViewControllerPresentationDelegate

- (void)notificationsViewControllerDidRemove:
    (NotificationsViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate notificationsCoordinatorDidRemove:self];
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
