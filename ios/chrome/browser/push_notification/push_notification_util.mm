// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_util.h"

#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PushNotificationUtil

+ (void)registerDeviceWithAPNS:(UIApplication*)application {
  [application registerForRemoteNotifications];
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