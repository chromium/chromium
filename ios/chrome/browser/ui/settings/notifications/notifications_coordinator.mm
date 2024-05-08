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
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_alert_coordinator.h"
#import "ios/chrome/browser/ui/settings/notifications/content_notifications/content_notifications_coordinator.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_banner_view_controller.h"
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
    ContentNotificationsCoordinatorDelegate,
    TrackingPriceCoordinatorDelegate,
    NotificationsOptInAlertCoordinatorDelegate,
    NotificationsBannerViewControllerPresentationDelegate>

// View controller presented by coordinator when feature IOSTipsNotifications is
// disabled.
@property(nonatomic, strong) NotificationsViewController* viewController;
// View controller presented by coordinator when feature IOSTipsNotifications is
// enabled.
@property(nonatomic, strong)
    NotificationsBannerViewController* updatedViewController;
// Notifications settings mediator.
@property(nonatomic, strong) NotificationsMediator* mediator;
// Coordinator for Content settings menu.
@property(nonatomic, strong)
    ContentNotificationsCoordinator* contentNotificationsCoordinator;
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
  _notificationsObserver = [[NotificationsSettingsObserver alloc]
      initWithPrefService:prefService
               localState:GetApplicationContext()->GetLocalState()];

  self.mediator = [[NotificationsMediator alloc] initWithPrefService:prefService
                                                              gaiaID:gaiaID];
  self.mediator.handler = self;
  self.mediator.presenter = self;
  _notificationsObserver.delegate = self.mediator;

  if (IsIOSTipsNotificationsEnabled()) {
    self.updatedViewController =
        [[NotificationsBannerViewController alloc] init];
    self.updatedViewController.presentationDelegate = self;
    self.updatedViewController.modelDelegate = self.mediator;
    self.mediator.consumer = self.updatedViewController;
    [self.baseNavigationController pushViewController:self.updatedViewController
                                             animated:YES];
  } else {
    self.viewController = [[NotificationsViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
    self.viewController.presentationDelegate = self;
    self.viewController.modelDelegate = self.mediator;
    self.mediator.consumer = self.viewController;
    [self.baseNavigationController pushViewController:self.viewController
                                             animated:YES];
  }
}

- (void)stop {
  _notificationsObserver.delegate = nil;
  [_notificationsObserver disconnect];
  _notificationsObserver = nil;
  [_optInAlertCoordinator stop];
}

#pragma mark - NotificationsAlertPresenter

- (void)presentTipsNotificationPermissionAlert {
  [_optInAlertCoordinator stop];
  UIViewController* baseViewController = IsIOSTipsNotificationsEnabled()
                                             ? self.updatedViewController
                                             : self.viewController;
  _optInAlertCoordinator = [[NotificationsOptInAlertCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:self.browser];
  _optInAlertCoordinator.clientIds =
      std::vector{PushNotificationClientId::kTips};
  _optInAlertCoordinator.alertMessage = l10n_util::GetNSString(
      IDS_IOS_TIPS_NOTIFICATIONS_SETTINGS_ALERT_SUBTITLE);
  _optInAlertCoordinator.delegate = self;
  [_optInAlertCoordinator start];
}

#pragma mark - NotificationsNavigationCommands

- (void)showContent {
  DCHECK(!self.contentNotificationsCoordinator);
  DCHECK(self.baseNavigationController);
  self.contentNotificationsCoordinator =
      [[ContentNotificationsCoordinator alloc]
          initWithBaseNavigationController:self.baseNavigationController
                                   browser:self.browser];
  self.contentNotificationsCoordinator.delegate = self;
  [self.contentNotificationsCoordinator start];
}

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

#pragma mark - NotificationsBannerViewControllerPresentationDelegate

- (void)notificationsBannerViewControllerDidRemove:
    (NotificationsBannerViewController*)controller {
  DCHECK_EQ(self.updatedViewController, controller);
  [self.delegate notificationsCoordinatorDidRemove:self];
}

#pragma mark - ContentNotificationsCoordinatorDelegate

- (void)contentNotificationsCoordinatorDidRemove:
    (ContentNotificationsCoordinator*)coordinator {
  DCHECK_EQ(self.contentNotificationsCoordinator, coordinator);
  [self.contentNotificationsCoordinator stop];
  self.contentNotificationsCoordinator.delegate = nil;
  self.contentNotificationsCoordinator = nil;
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
