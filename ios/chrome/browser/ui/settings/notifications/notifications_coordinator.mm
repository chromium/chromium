// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/notifications_coordinator.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_alert_coordinator.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_mediator.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_view_controller.h"
#import "ios/chrome/browser/ui/settings/notifications/tips_notifications_alert_presenter.h"
#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_coordinator.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface NotificationsCoordinator () <
    NotificationsNavigationCommands,
    NotificationsViewControllerPresentationDelegate,
    TrackingPriceCoordinatorDelegate,
    NotificationsOptInAlertCoordinatorDelegate>

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
@property(nonatomic, strong)
    NotificationsOptInAlertCoordinator* optInAlertCoordinator;

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
  self.mediator.presenter = self;
  _notificationsObserver.delegate = self.mediator;
  self.viewController.modelDelegate = self.mediator;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  _notificationsObserver.delegate = nil;
  _notificationsObserver = nil;
  [_optInAlertCoordinator stop];
}

#pragma mark - NotificationsAlertPresenter

- (void)presentPushNotificationPermissionAlert {
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = [[NotificationsOptInAlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _optInAlertCoordinator.clientIds =
      std::vector{PushNotificationClientId::kContent};
  _optInAlertCoordinator.alertMessage = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_SETTINGS_ALERT_MESSAGE);
  _optInAlertCoordinator.delegate = self;
  [_optInAlertCoordinator start];
}

- (void)presentTipsNotificationPermissionAlert {
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = [[NotificationsOptInAlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _optInAlertCoordinator.clientIds =
      std::vector{PushNotificationClientId::kTips};
  _optInAlertCoordinator.alertMessage = l10n_util::GetNSString(
      IDS_IOS_TIPS_NOTIFICATIONS_SETTINGS_ALERT_SUBTITLE);
  _optInAlertCoordinator.delegate = self;
  [_optInAlertCoordinator start];
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

#pragma mark - NotificationsOptInAlertCoordinatorDelegate

- (void)notificationsOptInAlertCoordinator:
            (NotificationsOptInAlertCoordinator*)alertCoordinator
                                    result:
                                        (NotificationsOptInAlertResult)result {
  CHECK_EQ(_optInAlertCoordinator, alertCoordinator);
  std::vector<PushNotificationClientId> clientIds =
      alertCoordinator.clientIds.value();
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = nil;
  switch (result) {
    case NotificationsOptInAlertResult::kPermissionDenied:
    case NotificationsOptInAlertResult::kCanceled:
    case NotificationsOptInAlertResult::kError:
    case NotificationsOptInAlertResult::kOpenedSettings:
      [_mediator deniedPermissionsForClientIds:std::move(clientIds)];
      break;
    case NotificationsOptInAlertResult::kPermissionGranted:
      break;
  }
}

@end
