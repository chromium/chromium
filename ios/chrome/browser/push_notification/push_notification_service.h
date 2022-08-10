// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_H_

#import "ios/chrome/browser/push_notification/push_notification_configuration.h"

// Service responsible for establishing connection and interacting
// with the push notification server.
class PushNotificationService {
 public:
  PushNotificationService();
  virtual ~PushNotificationService();

  // Initializes the device's connection and registers it to the push
  // notification server. `completion_handler` is invoked asynchronously when
  // the operation successfully or unsuccessfully completes.
  virtual void RegisterDevice(PushNotificationConfiguration* config,
                              void (^completion_handler)(NSError* error)) = 0;

  // Disassociates the device to its previously associated accounts on the push
  // notification server. `completion_handler` is invoked asynchronously when
  // the operation successfully or unsuccessfully completes.
  virtual void UnregisterDevice(void (^completion_handler)(NSError* error)) = 0;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_H_