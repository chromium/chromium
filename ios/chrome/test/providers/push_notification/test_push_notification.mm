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
  // Test implementation does nothing.
}

void RegisterDevice(NSData* device_token) {
  // Test implementation does nothing.
}

void RegisterDeviceWithAPNS(UIApplication* application) {
  // Test implementation does nothing.
}

void RequestPushNotificationPermission() {
  // Test implementation does noting.
}

}  // namespace provider
}  // namespace ios
