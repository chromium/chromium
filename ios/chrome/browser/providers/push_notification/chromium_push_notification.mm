// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

void InitializeConfiguration(id<SingleSignOnService> sso_service) {
  // Chromium does not initialize push notification configurations
}

void RegisterDevice(NSData* device_token) {
  // Chromium does not register devices for push notifications
}

void RegisterDeviceWithAPNS(UIApplication* application) {
  // Chromium does not register devices with Apple Push Notification Service
  // (APNS) for push notifications
}

void RequestPushNotificationPermission() {
  // Chromium does not request push notification permissions
}

}  // namespace provider
}  // namespace ios
