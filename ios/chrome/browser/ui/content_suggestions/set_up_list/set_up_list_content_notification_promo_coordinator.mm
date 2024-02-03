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
#import "ios/chrome/browser/ui/push_notification/notifications_alert_presenter.h"
#import "ios/chrome/browser/ui/push_notification/notifications_confirmation_presenter.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;

@interface SetUpListContentNotificationPromoCoordinator () <
    NotificationsAlertPresenter>

// Alert Coordinator used to display the notifications system prompt.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

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
  if (self.alertCoordinator) {
    [self dimissAlertCoordinator];
  }
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
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  RecordAction(
      UserMetricsAction("ContentNotifications.Promo.SetUpList.Accepted"));
  [self logHistogramForAction:ContentNotificationSetUpListPromoAction::kAccept];

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();
  service->SetPreference(identity.gaiaID, PushNotificationClientId::kContent,
                         true);
  _markItemComplete = YES;

  __weak SetUpListContentNotificationPromoCoordinator* weakSelf = self;
  [PushNotificationUtil requestPushNotificationPermission:^(
                            BOOL granted, BOOL promptShown, NSError* error) {
    if (!error && !promptShown && !granted) {
      // This callback can be executed on a background thread, make sure the UI
      // is displayed on the main thread.
      dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf presentPushNotificationPermissionAlert];
        [weakSelf logHistogramForEvent:ContentNotificationSetUpListPromoEvent::
                                           kPromptShown];
      });
    } else if (!error && granted) {
      dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf.messagePresenter presentNotificationsConfirmationMessage];
        [weakSelf.delegate setUpListContentNotificationPromoDidFinish];
      });
    } else {
      dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf.delegate setUpListContentNotificationPromoDidFinish];
      });
    }
  }];
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
  NSString* settingURL = UIApplicationOpenSettingsURLString;
  if (@available(iOS 15.4, *)) {
    settingURL = UIApplicationOpenNotificationSettingsURLString;
  }
  NSString* alertTitle = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_SETTINGS_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_SETTINGS_ALERT_MESSAGE);
  NSString* cancelTitle = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_PERMISSION_REDIRECT_ALERT_CANCEL);
  NSString* settingsTitle = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_PERMISSION_REDIRECT_ALERT_REDIRECT);

  __weak SetUpListContentNotificationPromoCoordinator* weakSelf = self;
  [self.alertCoordinator stop];
  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:_viewController
                                                   browser:self.browser
                                                     title:alertTitle
                                                   message:alertMessage];
  [self.alertCoordinator
      addItemWithTitle:cancelTitle
                action:^{
                  [weakSelf
                      logHistogramForPromptAction:
                          ContentNotificationPromptAction::kNoThanksTapped];
                  RecordAction(UserMetricsAction(
                      "ContentNotifications.Promo.Prompt.NoThanksTapped"));
                  [weakSelf
                          .delegate setUpListContentNotificationPromoDidFinish];
                }
                 style:UIAlertActionStyleCancel];
  [self.alertCoordinator
      addItemWithTitle:settingsTitle
                action:^{
                  [[UIApplication sharedApplication]
                                openURL:[NSURL URLWithString:settingURL]
                                options:{}
                      completionHandler:nil];
                  [weakSelf
                      logHistogramForPromptAction:
                          ContentNotificationPromptAction::kGoToSettingsTapped];
                  RecordAction(UserMetricsAction(
                      "ContentNotifications.Promo.Prompt.GoToSettingsTapped"));
                  [weakSelf
                          .delegate setUpListContentNotificationPromoDidFinish];
                }
                 style:UIAlertActionStyleDefault];
  [self.alertCoordinator start];
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

#pragma mark - Private

- (void)dimissAlertCoordinator {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

@end
