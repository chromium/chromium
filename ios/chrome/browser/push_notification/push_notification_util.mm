// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_util.h"

#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    (void (^)(bool granted, NSError* error))completionHandler {
  UNAuthorizationOptions options = UNAuthorizationOptionAlert |
                                   UNAuthorizationOptionBadge |
                                   UNAuthorizationOptionSound;

  UNUserNotificationCenter* center =
      UNUserNotificationCenter.currentNotificationCenter;
  [center requestAuthorizationWithOptions:options
                        completionHandler:completionHandler];
}

+ (void)getPermissionSettings:
    (void (^)(UNNotificationSettings* settings))completionHandler {
  UNUserNotificationCenter* center =
      UNUserNotificationCenter.currentNotificationCenter;
  [center getNotificationSettingsWithCompletionHandler:completionHandler];
}

@end