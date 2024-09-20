// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_UTIL_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_UTIL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

// This util holds class methods to update the status of provisional push
// notifications.
@interface ProvisionalPushNotificationUtil : NSObject

// This function enrolls the user into provisional notifications for a specific
// feature or series of features. Checks if notifications have been previously
// authorized or denied, and if not, enables provisional notifications for the
// user.
+ (void)enrollUserToProvisionalNotificationsForClientIds:
            (std::vector<PushNotificationClientId>)clientIds
                             clientEnabledForProvisional:
                                 (BOOL)clientEnabledForProvisional
                                         withAuthService:
                                             (AuthenticationService*)authService
                                   deviceInfoSyncService:
                                       (syncer::DeviceInfoSyncService*)
                                           deviceInfoSyncService;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_UTIL_H_
