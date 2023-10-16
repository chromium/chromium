// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_util.h"

#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

#import "base/metrics/histogram_functions.h"

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

enum class PushNotificationSettingsAuthorizationStatus {
  NOTDETERMINED,
  DENIED,
  AUTHORIZED,
  PROVISIONAL,
  EPHEMERAL,
  kMaxValue = EPHEMERAL
};

// The histogram used to record the outcome of the permission prompt.
const char kEnabledPermissionsHistogram[] =
    "IOS.PushNotification.EnabledPermisisons";

// The histogram used to record the user's push notification authorization
// status.
const char kAuthorizationStatusHistogram[] =
    "IOS.PushNotification.NotificationSettingsAuthorizationStatus";
}  // namespace

@implementation PushNotificationUtil

+ (void)registerDeviceWithAPNS {
  [PushNotificationUtil
      getPermissionSettings:^(UNNotificationSettings* settings) {
        // Logs the users iOS settings' push notification permission status over
        // time.
        [PushNotificationUtil
            logPermissionSettingsMetrics:settings.authorizationStatus];

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
  switch (authorizationStatus) {
    case UNAuthorizationStatusNotDetermined:
      // The authorization status is this case when the user has not yet
      // decided to give Chrome push notification permissions.
      base::UmaHistogramEnumeration(
          kAuthorizationStatusHistogram,
          PushNotificationSettingsAuthorizationStatus::NOTDETERMINED);
      break;
    case UNAuthorizationStatusDenied:
      // The authorization status is this case when the user has denied to
      // give Chrome push notification permissions via the push
      // notification iOS system permission prompt or by navigating to the iOS
      // settings and manually enabling it.
      base::UmaHistogramEnumeration(
          kAuthorizationStatusHistogram,
          PushNotificationSettingsAuthorizationStatus::DENIED);
      break;
    case UNAuthorizationStatusAuthorized:
      // The authorization status is this case when the user has
      // authorized to give Chrome push notification permissions via the
      // push notification iOS system permission prompt or by navigating to the
      // iOS settings and manually enabling it.
      base::UmaHistogramEnumeration(
          kAuthorizationStatusHistogram,
          PushNotificationSettingsAuthorizationStatus::AUTHORIZED);
      break;
    case UNAuthorizationStatusProvisional:
      // The authorization status is this case when Chrome has the ability
      // to send provisional push notifications.
      base::UmaHistogramEnumeration(
          kAuthorizationStatusHistogram,
          PushNotificationSettingsAuthorizationStatus::PROVISIONAL);
      break;
    case UNAuthorizationStatusEphemeral:
      // The authorization status is this case Chrome can receive
      // notifications for a limited amount of time.
      base::UmaHistogramEnumeration(
          kAuthorizationStatusHistogram,
          PushNotificationSettingsAuthorizationStatus::EPHEMERAL);
      break;
  }

  // TODO(crbug.com/1487295): Add metric that tracks when users changes
  // their push notification permission authorization status to
  // Authorized/Denied.
}

@end
