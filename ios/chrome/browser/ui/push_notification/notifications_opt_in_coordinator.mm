// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/push_notification/metrics.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_alert_coordinator.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_coordinator_delegate.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_item_identifier.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_mediator.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_presenter.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface NotificationsOptInCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    NotificationsOptInPresenter,
    NotificationsOptInAlertCoordinatorDelegate>

// Mediator for NotificationsOptInViewController.
@property(nonatomic, strong) NotificationsOptInMediator* mediator;

@end

@implementation NotificationsOptInCoordinator {
  // View controller for the Notifications Opt-In screen.
  NotificationsOptInViewController* _viewController;
  // Coordinator that presents the notifications opt-in alert.
  NotificationsOptInAlertCoordinator* _optInAlertCoordinator;
}

- (void)start {
  _viewController = [[NotificationsOptInViewController alloc] init];
  NotificationsOptInMediator* mediator = [[NotificationsOptInMediator alloc]
      initWithAuthenticationService:AuthenticationServiceFactory::GetForProfile(
                                        self.browser->GetProfile())];
  mediator.consumer = _viewController;
  mediator.presenter = self;
  _viewController.delegate = mediator;
  _viewController.notificationsDelegate = mediator;
  _viewController.presentationController.delegate = self;
  _viewController.isContentNotificationEnabled =
      IsContentNotificationEnabled(self.browser->GetProfile());
  [mediator configureConsumer];
  self.mediator = mediator;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [_viewController.presentingViewController dismissViewControllerAnimated:NO
                                                               completion:nil];
  _viewController = nil;
  self.mediator = nil;
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = nil;
}

#pragma mark - NotificationsOptInPresenter

- (void)presentSignIn {
  __weak __typeof(self) weakSelf = self;
  ShowSigninCommandCompletionCallback callback =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* completionInfo) {
        if (result != SigninCoordinatorResultSuccess) {
          [weakSelf.mediator disableUserSelectionForItem:kContent];
        }
      };
  // If there are 0 identities, kInstantSignin requires less taps.
  ProfileIOS* profile = self.browser->GetProfile();
  AuthenticationOperation operation =
      ChromeAccountManagerServiceFactory::GetForProfile(profile)
              ->HasIdentities()
          ? AuthenticationOperation::kSigninOnly
          : AuthenticationOperation::kInstantSignin;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:operation
               identity:nil
            accessPoint:
                signin_metrics::AccessPoint::
                    ACCESS_POINT_NOTIFICATIONS_OPT_IN_SCREEN_CONTENT_TOGGLE
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:callback];
  [HandlerForProtocol(self.browser->GetCommandDispatcher(), ApplicationCommands)
              showSignin:command
      baseViewController:_viewController];
}

- (void)presentNotificationsAlertForClientIds:
    (std::vector<PushNotificationClientId>)clientIds {
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = [[NotificationsOptInAlertCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  _optInAlertCoordinator.delegate = self;
  _optInAlertCoordinator.clientIds = clientIds;
  [_optInAlertCoordinator start];
}

- (void)dismiss {
  [self dismissViewController];
}

#pragma mark - NotificationsOptInAlertCoordinatorDelegate

- (void)notificationsOptInAlertCoordinator:
            (NotificationsOptInAlertCoordinator*)alertCoordinator
                                    result:
                                        (NotificationsOptInAlertResult)result {
  CHECK_EQ(_optInAlertCoordinator, alertCoordinator);
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = nil;
  switch (result) {
    case NotificationsOptInAlertResult::kPermissionGranted:
      [self dismissViewController];
      [self recordNotificationsEnabledForClients:alertCoordinator.clientIds
                                                     .value()];
      break;
    case NotificationsOptInAlertResult::kPermissionDenied:
    case NotificationsOptInAlertResult::kOpenedSettings:
    case NotificationsOptInAlertResult::kCanceled:
    case NotificationsOptInAlertResult::kError:
      break;
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate notificationsOptInScreenDidFinish:self];
  base::UmaHistogramEnumeration(
      kNotificationsOptInPromptActionHistogram,
      NotificationsOptInPromptActionType::kSwipedToDismiss);
}

#pragma mark - Private

// Dismisses the base view controller.
- (void)dismissViewController {
  __weak __typeof(self) weakSelf = self;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf.delegate
                               notificationsOptInScreenDidFinish:weakSelf];
                         }];
}

// Records notifications that are enabled through the prompt.
- (void)recordNotificationsEnabledForClients:
    (std::vector<PushNotificationClientId>)clientIds {
  for (PushNotificationClientId clientId : clientIds) {
    switch (clientId) {
      case PushNotificationClientId::kCommerce:
        base::RecordAction(base::UserMetricsAction(
            kNotificationsOptInPromptPriceTrackingEnabled));
        break;
      case PushNotificationClientId::kTips:
        base::RecordAction(
            base::UserMetricsAction(kNotificationsOptInPromptTipsEnabled));
        break;
      case PushNotificationClientId::kContent:
        base::RecordAction(
            base::UserMetricsAction(kNotificationsOptInPromptContentEnabled));
        break;
      case PushNotificationClientId::kSports:
        // Content and sports are enabled together.
        break;
      case PushNotificationClientId::kSafetyCheck:
        base::RecordAction(base::UserMetricsAction(
            kNotificationsOptInPromptSafetyCheckEnabled));
        break;
      case PushNotificationClientId::kSendTab:
        base::RecordAction(
            base::UserMetricsAction(kNotificationsOptInPromptSendTabEnabled));
        break;
    }
  }
}

@end
