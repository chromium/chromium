// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_API_H_

#import <memory>

#import "ios/chrome/browser/push_notification/model/push_notification_service.h"

namespace ios {
namespace provider {

// Creates a new instance of PushNotificationService.
std::unique_ptr<PushNotificationService> CreatePushNotificationService();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_API_H_
