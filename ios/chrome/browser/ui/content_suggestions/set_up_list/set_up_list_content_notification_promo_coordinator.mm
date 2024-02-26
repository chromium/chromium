// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_coordinator.h"

#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_view_controller.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_alert_coordinator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;

@interface SetUpListContentNotificationPromoCoordinator () <
    NotificationsOptInAlertCoordinatorDelegate>

// Alert Coordinator used to display the notifications system prompt.
@property(nonatomic, strong)
    NotificationsOptInAlertCoordinator* optInAlertCoordinator;
@end

@implementation SetUpListContentNotificationPromoCoordinator {
  // The view controller that displays the content notification promo.
  SetUpListContentNotificationPromoViewController* _viewController;

  // Application is used to open the OS settings for this app.
  UIApplication* _application;

  // Whether or not the Set Up List Item should be marked complete.
  BOOL _markItemComplete;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               application:(UIApplication*)application {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _application = application;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController =
      [[SetUpListContentNotificationPromoViewController alloc] init];
  _viewController.delegate = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
  _viewController.presentationController.delegate = self;
  [self logHistogramForEvent:ContentNotificationSetUpListPromoEvent::kShown];
}

- (void)stop {
  _viewController.presentationController.delegate = nil;

  ProceduralBlock completion = nil;
  if (_markItemComplete) {
    PrefService* localState = GetApplicationContext()->GetLocalState();
    completion = ^{
      set_up_list_prefs::MarkItemComplete(localState,
                                          SetUpListItemType::kNotifications);
    };
  }
  [_viewController dismissViewControllerAnimated:YES completion:completion];
  [self
      logHistogramForEvent:ContentNotificationSetUpListPromoEvent::kDismissed];
  _viewController = nil;
  self.delegate = nil;
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  RecordAction(
      UserMetricsAction("ContentNotifications.Promo.SetUpList.Accepted"));
  [self logHistogramForAction:ContentNotificationSetUpListPromoAction::kAccept];
  [self presentPushNotificationPermissionAlert];
  _markItemComplete = YES;
}

- (void)didTapSecondaryActionButton {
  RecordAction(
      UserMetricsAction("ContentNotifications.Promo.SetUpList.Canceled"));
  [self logHistogramForAction:ContentNotificationSetUpListPromoAction::kCancel];
  _markItemComplete = YES;
  [self.delegate setUpListContentNotificationPromoDidFinish];
}

- (void)didTapTertiaryActionButton {
  RecordAction(
      UserMetricsAction("ContentNotifications.Promo.SetUpList.Uncompleted"));
  [self logHistogramForAction:ContentNotificationSetUpListPromoAction::
                                  kRemindMeLater];

  [self.delegate setUpListContentNotificationPromoDidFinish];
}

#pragma mark - NotificationsAlertPresenter

- (void)presentPushNotificationPermissionAlert {
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = [[NotificationsOptInAlertCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  _optInAlertCoordinator.clientIds = std::vector{
      PushNotificationClientId::kContent, PushNotificationClientId::kSports};
  _optInAlertCoordinator.alertMessage = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_SETTINGS_ALERT_MESSAGE);
  _optInAlertCoordinator.confirmationMessage =
      l10n_util::GetNSString(IDS_IOS_CONTENT_NOTIFICATION_SNACKBAR_TITLE);
  _optInAlertCoordinator.delegate = self;
  [_optInAlertCoordinator start];
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
  [self logHistogramForEvent:ContentNotificationSetUpListPromoEvent::
                                 kPromptShown];
  switch (result) {
    case NotificationsOptInAlertResult::kPermissionDenied:
      [self logHistogramForPromptAction:ContentNotificationPromptAction::
                                            kNoThanksTapped];
      break;
    case NotificationsOptInAlertResult::kCanceled:
    case NotificationsOptInAlertResult::kError:
      break;
    case NotificationsOptInAlertResult::kOpenedSettings:
      [self logHistogramForPromptAction:ContentNotificationPromptAction::
                                            kGoToSettingsTapped];
      break;
    case NotificationsOptInAlertResult::kPermissionGranted:
      [self logHistogramForPromptAction:ContentNotificationPromptAction::
                                            kAccepted];
      break;
  }
  [self.delegate setUpListContentNotificationPromoDidFinish];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate setUpListContentNotificationPromoDidFinish];
}

#pragma mark - Metrics Helpers

- (void)logHistogramForAction:(ContentNotificationSetUpListPromoAction)action {
  UmaHistogramEnumeration("ContentNotifications.Promo.SetUpList.Action",
                          action);
}

- (void)logHistogramForPromptAction:(ContentNotificationPromptAction)action {
  UmaHistogramEnumeration("ContentNotifications.Promo.Prompt.Action", action);
}

- (void)logHistogramForEvent:(ContentNotificationSetUpListPromoEvent)event {
  UmaHistogramEnumeration("ContentNotifications.Promo.SetUpList.Event", event);
}

@end
