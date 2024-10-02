// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_alert_coordinator.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/ui/push_notification/metrics.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Returns the gaia id used for `profile`.
NSString* GetGaiaIdForProfile(ProfileIOS* profile) {
  const ProfileAttributesIOS attributes =
      GetApplicationContext()
          ->GetProfileManager()
          ->GetProfileAttributesStorage()
          ->GetAttributesForProfileWithName(profile->GetProfileName());

  return base::SysUTF8ToNSString(attributes.GetGaiaId());
}

}  // namespace

@implementation NotificationsOptInAlertCoordinator {
  SEQUENCE_CHECKER(sequence_checker_);
  // The coordinator used to present the alert used when permission has
  // previously been denied.
  AlertCoordinator* _alertCoordinator;
}

- (void)start {
  CHECK(self.clientIds.has_value());

  [self requestPushNotificationPermission];
}

- (void)stop {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

#pragma mark - Private methods

// Asks iOS to request permission from the user to receive notifications. If
// the request has already happened and permission was not granted, an alert
// will be presented to ask the user if they want to enable the permission in
// the iOS settings app.
- (void)requestPushNotificationPermission {
  __weak __typeof(self) weakSelf = self;
  scoped_refptr<base::TaskRunner> taskRunner =
      base::SequencedTaskRunner::GetCurrentDefault();
  [PushNotificationUtil requestPushNotificationPermission:^(
                            BOOL granted, BOOL promptShown, NSError* error) {
    taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                           [weakSelf onRequestPermissionResult:granted
                                                   promptShown:promptShown
                                                         error:error];
                         }));
  }];
}

// Handles the response from requesting notification authorization.
- (void)onRequestPermissionResult:(BOOL)granted
                      promptShown:(BOOL)promptShown
                            error:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    [self setResult:NotificationsOptInAlertResult::kError];
  } else if (!granted) {
    if (!promptShown) {
      [self presentNotificationPermissionAlert];
    } else {
      [self setResult:NotificationsOptInAlertResult::kPermissionDenied];
    }
  } else {
    // Permission has been granted!
    [self enableNotifications];
    if (self.confirmationMessage) {
      [self showConfirmationSnackbar];
    }
    [self setResult:NotificationsOptInAlertResult::kPermissionGranted];
  }
}

// Presents an alert view to ask the user to enable notifications via iOS
// settings.
- (void)presentNotificationPermissionAlert {
  NSString* alertTitle =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_ALERT_TITLE);
  NSString* alertMessage =
      self.alertMessage
          ? self.alertMessage
          : l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_ALERT_MESSAGE);
  NSString* cancelTitle =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_ALERT_CANCEL);
  NSString* settingsTitle =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_ALERT_GO_TO_SETTINGS);

  [_alertCoordinator stop];
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:alertTitle
                         message:alertMessage];
  __weak __typeof(self) weakSelf = self;
  [_alertCoordinator addItemWithTitle:cancelTitle
                               action:^{
                                 [weakSelf didCancelAlert];
                               }
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:settingsTitle
                               action:^{
                                 [weakSelf openSettings];
                               }
                                style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

// Enables notifications in prefs for the client with `clientID`.
- (void)enableNotifications {
  NSString* gaiaID = GetGaiaIdForProfile(self.browser->GetProfile());
  std::vector<PushNotificationClientId> clientIDs = self.clientIds.value();
  for (PushNotificationClientId clientID : clientIDs) {
    GetApplicationContext()->GetPushNotificationService()->SetPreference(
        gaiaID, clientID, true);
    if (clientID == PushNotificationClientId::kSendTab) {
      // Refresh enabled status in DeviceInfo.
      DeviceInfoSyncServiceFactory::GetForProfile(self.browser->GetProfile())
          ->RefreshLocalDeviceInfo();
    }
  }
}

// Shows a snackbar message indicating that notifications are enabled.
- (void)showConfirmationSnackbar {
  NSString* buttonText =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_MANAGE_SETTINGS);
  // Show snackbar confirmation.

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  __weak id<SettingsCommands> weakSettingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  __weak id<ApplicationCommands> weakApplicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  [snackbarHandler
      showSnackbarWithMessage:self.confirmationMessage
                   buttonText:buttonText
                messageAction:^{
                  [weakApplicationHandler prepareToPresentModal:^{
                    [weakSettingsHandler showNotificationsSettings];
                  }];
                }
             completionAction:nil];
}

// Opens the iOS settings app to the app's Notification permissions.
- (void)openSettings {
  NSURL* url = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
  if (@available(iOS 15.4, *)) {
    url = [NSURL URLWithString:UIApplicationOpenNotificationSettingsURLString];
  }

  [[UIApplication sharedApplication] openURL:url
                                     options:@{}
                           completionHandler:nil];
  [self setResult:NotificationsOptInAlertResult::kOpenedSettings];
}

// Called when the user taps the alert's "cancel" action.
- (void)didCancelAlert {
  [self setResult:NotificationsOptInAlertResult::kCanceled];
}

// Tells the delegate the result of the UI flow.
- (void)setResult:(NotificationsOptInAlertResult)result {
  [self.delegate notificationsOptInAlertCoordinator:self result:result];
  switch (result) {
    case NotificationsOptInAlertResult::kPermissionGranted:
      base::RecordAction(
          base::UserMetricsAction(kNotificationsOptInAlertPermissionGranted));
      break;
    case NotificationsOptInAlertResult::kPermissionDenied:
      base::RecordAction(
          base::UserMetricsAction(kNotificationsOptInAlertPermissionDenied));
      break;
    case NotificationsOptInAlertResult::kOpenedSettings:
      base::RecordAction(
          base::UserMetricsAction(kNotificationsOptInAlertOpenedSettings));
      break;
    case NotificationsOptInAlertResult::kCanceled:
      base::RecordAction(
          base::UserMetricsAction(kNotificationsOptInAlertCancelled));
      break;
    case NotificationsOptInAlertResult::kError:
      base::RecordAction(
          base::UserMetricsAction(kNotificationsOptInAlertError));
      break;
  }
}

@end
