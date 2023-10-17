// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_util.h"

#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace {

using PermissionResponseHandler = void (^)(BOOL granted,
                                           BOOL promptedUser,
                                           NSError* error);

// This enum is used to record the action a user performed when prompted to
// allow push notification permissions.
enum class PermissionPromptAction {
  ACCEPTED,
  DECLINED,
  ERROR,
  kMaxValue = ERROR
};

// The histogram used to record the outcome of the permission prompt.
const char kEnabledPermissionsHistogram[] =
    "IOS.PushNotification.EnabledPermisisons";

// The histogram used to record the user's push notification authorization
// status.
const char kAuthorizationStatusHistogram[] =
    "IOS.PushNotification.NotificationSettingsAuthorizationStatus";

// The histogram used to record users changes to an authorized push notification
// permission status.
const char kNotificationAutorizationStatusChangedToAuthorized[] =
    "IOS.PushNotification.NotificationAutorizationStatusChangedToAuthorized";

// The histogram used to record users changes to a denied push notification
// permission status.
const char kNotificationAutorizationStatusChangedToDenied[] =
    "IOS.PushNotification.NotificationAutorizationStatusChangedToDenied";
}  // namespace

@implementation PushNotificationUtil

+ (void)registerDeviceWithAPNS {
  [PushNotificationUtil
      getPermissionSettings:^(UNNotificationSettings* settings) {
        dispatch_async(dispatch_get_main_queue(), ^{
          // Logs the users iOS settings' push notification permission status
          // over time.
          [PushNotificationUtil
              logPermissionSettingsMetrics:settings.authorizationStatus];
        });

        if (settings.authorizationStatus == UNAuthorizationStatusAuthorized) {
          // iOS instructs that registering the device with APNS must be done on
          // the main thread. Otherwise, a runtime warning is generated.
          dispatch_async(dispatch_get_main_queue(), ^{
            [[UIApplication sharedApplication] registerForRemoteNotifications];
          });
        }
      }];
}

+ (void)registerActionableNotifications:
    (NSSet<UNNotificationCategory*>*)categories {
  UNUserNotificationCenter* center =
      UNUserNotificationCenter.currentNotificationCenter;
  [center setNotificationCategories:categories];
}

+ (void)requestPushNotificationPermission:
    (PermissionResponseHandler)completionHandler {
  [PushNotificationUtil getPermissionSettings:^(
                            UNNotificationSettings* settings) {
    [PushNotificationUtil requestPushNotificationPermission:completionHandler
                                         permissionSettings:settings];
  }];
}

+ (void)getPermissionSettings:
    (void (^)(UNNotificationSettings* settings))completionHandler {
  UNUserNotificationCenter* center =
      UNUserNotificationCenter.currentNotificationCenter;
  [center getNotificationSettingsWithCompletionHandler:completionHandler];
}

#pragma mark - Private

// Displays the push notification permission prompt if the user has not decided
// on the application's permission status.
+ (void)requestPushNotificationPermission:(PermissionResponseHandler)completion
                       permissionSettings:(UNNotificationSettings*)settings {
  if (settings.authorizationStatus != UNAuthorizationStatusNotDetermined) {
    if (completion) {
      completion(
          settings.authorizationStatus == UNAuthorizationStatusAuthorized, NO,
          nil);
    }
    return;
  }
  UNAuthorizationOptions options = UNAuthorizationOptionAlert |
                                   UNAuthorizationOptionBadge |
                                   UNAuthorizationOptionSound;
  UNUserNotificationCenter* center =
      UNUserNotificationCenter.currentNotificationCenter;
  [center requestAuthorizationWithOptions:options
                        completionHandler:^(BOOL granted, NSError* error) {
                          [PushNotificationUtil
                              requestAuthorizationResult:completion
                                                 granted:granted
                                                   error:error];
                        }];
}

// Reports the push notification permission prompt's outcome to metrics.
+ (void)requestAuthorizationResult:(PermissionResponseHandler)completion
                           granted:(BOOL)granted
                             error:(NSError*)error {
  if (granted) {
    [PushNotificationUtil registerDeviceWithAPNS];
    base::UmaHistogramEnumeration(kEnabledPermissionsHistogram,
                                  PermissionPromptAction::ACCEPTED);
  } else if (!error) {
    base::UmaHistogramEnumeration(kEnabledPermissionsHistogram,
                                  PermissionPromptAction::DECLINED);
  } else {
    base::UmaHistogramEnumeration(kEnabledPermissionsHistogram,
                                  PermissionPromptAction::ERROR);
  }

  if (completion) {
    completion(granted, YES, error);
  }
}

// Logs the permission status, stored in iOS settings, the user has given for
// whether Chrome can receive push notifications on the device to UMA.
+ (void)logPermissionSettingsMetrics:
    (UNAuthorizationStatus)authorizationStatus {
  push_notification::PushNotificationSettingsAuthorizationStatus status;
  switch (authorizationStatus) {
    case UNAuthorizationStatusNotDetermined:
      // The authorization status is this case when the user has not yet
      // decided to give Chrome push notification permissions.
      status = push_notification::PushNotificationSettingsAuthorizationStatus::
          NOTDETERMINED;
      break;
    case UNAuthorizationStatusDenied:
      // The authorization status is this case when the user has denied to
      // give Chrome push notification permissions via the push
      // notification iOS system permission prompt or by navigating to the iOS
      // settings and manually enabling it.
      status = push_notification::PushNotificationSettingsAuthorizationStatus::
          DENIED;
      break;
    case UNAuthorizationStatusAuthorized:
      // The authorization status is this case when the user has
      // authorized to give Chrome push notification permissions via the
      // push notification iOS system permission prompt or by navigating to the
      // iOS settings and manually enabling it.
      status = push_notification::PushNotificationSettingsAuthorizationStatus::
          AUTHORIZED;
      break;
    case UNAuthorizationStatusProvisional:
      // The authorization status is this case when Chrome has the ability
      // to send provisional push notifications.
      status = push_notification::PushNotificationSettingsAuthorizationStatus::
          PROVISIONAL;
      break;
    case UNAuthorizationStatusEphemeral:
      // The authorization status is this case Chrome can receive
      // notifications for a limited amount of time.
      status = push_notification::PushNotificationSettingsAuthorizationStatus::
          EPHEMERAL;
      break;
  }
  base::UmaHistogramEnumeration(kAuthorizationStatusHistogram, status);

  ApplicationContext* context = GetApplicationContext();
  PrefService* prefService = context->GetLocalState();
  int previousAuthorizationStatus =
      prefService->GetInteger(prefs::kPushNotificationAuthorizationStatus);
  if (previousAuthorizationStatus != static_cast<int>(status)) {
    if (status == push_notification::
                      PushNotificationSettingsAuthorizationStatus::AUTHORIZED) {
      base::UmaHistogramEnumeration(
          kNotificationAutorizationStatusChangedToAuthorized,
          static_cast<
              push_notification::PushNotificationSettingsAuthorizationStatus>(
              previousAuthorizationStatus));
    } else if (status ==
               push_notification::PushNotificationSettingsAuthorizationStatus::
                   DENIED) {
      base::UmaHistogramEnumeration(
          kNotificationAutorizationStatusChangedToDenied,
          static_cast<
              push_notification::PushNotificationSettingsAuthorizationStatus>(
              previousAuthorizationStatus));
    }

    prefService->SetInteger(prefs::kPushNotificationAuthorizationStatus,
                            static_cast<int>(status));
  }
}

@end
