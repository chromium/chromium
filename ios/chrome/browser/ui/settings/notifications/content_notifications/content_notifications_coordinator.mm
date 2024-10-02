// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/content_notifications/content_notifications_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_alert_coordinator.h"
#import "ios/chrome/browser/ui/settings/notifications/content_notifications/content_notifications_mediator.h"
#import "ios/chrome/browser/ui/settings/notifications/content_notifications/content_notifications_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface ContentNotificationsCoordinator () <
    ContentNotificationsViewControllerPresentationDelegate,
    NotificationsOptInAlertCoordinatorDelegate>

// View controller presented by coordinator.
@property(nonatomic, strong) ContentNotificationsViewController* viewController;
// ContentNotifications settings mediator.
@property(nonatomic, strong) ContentNotificationsMediator* mediator;
// Alert Coordinator used to display the notifications system prompt.
@property(nonatomic, strong)
    NotificationsOptInAlertCoordinator* optInAlertCoordinator;

@end

@implementation ContentNotificationsCoordinator

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
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  const std::string& gaiaID = base::SysNSStringToUTF8(identity.gaiaID);
  PrefService* prefService = self.browser->GetProfile()->GetPrefs();

  self.viewController = [[ContentNotificationsViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.presentationDelegate = self;
  self.mediator =
      [[ContentNotificationsMediator alloc] initWithPrefService:prefService
                                                         gaiaID:gaiaID];
  ContentNotificationService* contentNotificationService =
      ContentNotificationServiceFactory::GetForProfile(
          self.browser->GetProfile());
  self.mediator.contentNotificationService = contentNotificationService;
  self.mediator.consumer = self.viewController;
  self.mediator.presenter = self;
  self.viewController.modelDelegate = self.mediator;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [_optInAlertCoordinator stop];
}

#pragma mark - ContentNotificationsViewControllerPresentationDelegate

- (void)contentNotificationsViewControllerDidRemove:
    (ContentNotificationsViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate contentNotificationsCoordinatorDidRemove:self];
}

#pragma mark - NotificationsAlertPresenter

- (void)presentPushNotificationPermissionAlertWithClientIds:
    (std::vector<PushNotificationClientId>)clientIds {
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = [[NotificationsOptInAlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _optInAlertCoordinator.clientIds = clientIds;
  _optInAlertCoordinator.alertMessage = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_SETTINGS_ALERT_MESSAGE);
  _optInAlertCoordinator.delegate = self;
  [_optInAlertCoordinator start];
}

- (void)presentPushNotificationPermissionAlert {
  NOTREACHED_IN_MIGRATION();
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
