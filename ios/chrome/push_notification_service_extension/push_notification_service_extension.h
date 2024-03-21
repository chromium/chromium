// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_PUSH_NOTIFICATION_SERVICE_EXTENSION_PUSH_NOTIFICATION_SERVICE_EXTENSION_H_
#define IOS_CHROME_PUSH_NOTIFICATION_SERVICE_EXTENSION_PUSH_NOTIFICATION_SERVICE_EXTENSION_H_

#import <UserNotifications/UserNotifications.h>

@interface PushNotificationServiceExtension : UNNotificationServiceExtension

- (instancetype)init NS_DESIGNATED_INITIALIZER;

// Overrides the `didReceiveNotificationRequest:` to be able to support
// content-rich notifications.
- (void)didReceiveNotificationRequest:(UNNotificationRequest*)request
                   withContentHandler:
                       (void (^)(UNNotificationContent* contentToDeliver))
                           contentHandler;

@end

#endif  // IOS_CHROME_PUSH_NOTIFICATION_SERVICE_EXTENSION_PUSH_NOTIFICATION_SERVICE_EXTENSION_H_
