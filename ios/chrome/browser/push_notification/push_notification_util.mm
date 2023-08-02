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

// The histogram used to record the outcome of the permission prompt
const char kEnabledPermissionsHistogram[] =
    "IOS.PushNotification.EnabledPermisisons";

}  // namespace

@implementation PushNotificationUtil

+ (void)registerDeviceWithAPNS {
  [PushNotificationUtil
      getPermissionSettings:^(UNNotificationSettings* settings) {
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

@end
