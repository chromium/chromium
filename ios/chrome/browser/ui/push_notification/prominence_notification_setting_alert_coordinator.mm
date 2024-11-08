// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/push_notification/prominence_notification_setting_alert_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/push_notification/prominence_notification_setting_alert_coordinator_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const char kPushNotificationProminentAlertHistogram[] =
    "IOS.PushNotification.ProminenceAlert.Result";

// Interactions with the Notifications Opt-In prompt. This is mapped to
// the IOSNotificationsProminenceAlertActionResult enum in enums.xml for
// metrics.
// LINT.IfChange(NotificationsProminenceAlertActionType)
enum class NotificationsProminenceAlertActionType {
  kOpenSettingsTapped = 0,
  kNoThanksTapped = 1,
  kMaxValue = kNoThanksTapped,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

}  // namespace

@implementation ProminenceNotificationSettingAlertCoordinator {
  // Underlying view controller
  AlertViewController* _alertViewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  const int impressionCount = localState->GetInteger(
      prefs::kProminenceNotificationAlertImpressionCount);
  localState->SetInteger(prefs::kProminenceNotificationAlertImpressionCount,
                         impressionCount + 1);
  _alertViewController = [[AlertViewController alloc] init];
  _alertViewController.modalPresentationStyle =
      UIModalPresentationOverFullScreen;
  _alertViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  [self configureAlert];
  [self.baseViewController presentViewController:_alertViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_alertViewController dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - Private

- (void)openAppNotificationSettings {
  __weak __typeof(self) weakSelf = self;
  [[UIApplication sharedApplication]
      openURL:[NSURL
                  URLWithString:UIApplicationOpenNotificationSettingsURLString]
      options:{}
      completionHandler:^(BOOL) {
        [weakSelf dismiss];
      }];
}

- (void)dismiss {
  [self.delegate prominenceNotificationSettingAlertCoordinatorIsDone:self];
}

- (void)configureAlert {
  [_alertViewController
      setTitle:l10n_util::GetNSString(
                   IDS_IOS_PROMINENCE_NOTIFICATION_SETTINGS_ALERT_TITLE)];
  [_alertViewController
      setMessage:
          l10n_util::GetNSString(
              IDS_IOS_PROMINENCE_NOTIFICATION_SETTINGS_ALERT_DECRIPTION)];
  [_alertViewController
      setImageLottieName:@"prominence_notification_light_mode"
      darkModeLottieName:@"prominence_notification_dark_mode"];

  __weak __typeof(self) weakSelf = self;
  AlertAction* openSettingsAction = [AlertAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_PROMINENCE_NOTIFICATION_SETTINGS_OPEN_SETTINGS_BUTTON)
                style:UIAlertActionStyleCancel
              handler:^(AlertAction* action) {
                base::UmaHistogramEnumeration(
                    kPushNotificationProminentAlertHistogram,
                    NotificationsProminenceAlertActionType::
                        kOpenSettingsTapped);
                [weakSelf openAppNotificationSettings];
              }];
  AlertAction* noThanksAction = [AlertAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_PROMINENCE_NOTIFICATION_SETTINGS_NO_THANKS_BUTTON)
                style:UIAlertActionStyleDefault
              handler:^(AlertAction* action) {
                base::UmaHistogramEnumeration(
                    kPushNotificationProminentAlertHistogram,
                    NotificationsProminenceAlertActionType::kNoThanksTapped);
                [weakSelf dismiss];
              }];
  [_alertViewController
      setActions:@[ @[ openSettingsAction ], @[ noThanksAction ] ]];
}

@end
