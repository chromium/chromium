// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/provisional_push_notification_util.h"

#import <UserNotifications/UserNotifications.h>

#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation ProvisionalPushNotificationUtil

+ (void)enrollUserToProvisionalNotificationsForClientIds:
            (std::vector<PushNotificationClientId>)clientIds
                             clientEnabledForProvisional:
                                 (BOOL)clientEnabledForProvisional
                                         withAuthService:
                                             (AuthenticationService*)authService
                                   deviceInfoSyncService:
                                       (syncer::DeviceInfoSyncService*)
                                           deviceInfoSyncService {
  if (authService &&
      authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    // Only users with a "Not Determined" (`UNAuthorizationStatusNotDetermined`)
    // or "Provisional" (`UNAuthorizationStatusProvisional`) notification
    // authorization status are eligible for provisional notifications.
    [PushNotificationUtil getPermissionSettings:^(
                              UNNotificationSettings* settings) {
      if (settings.authorizationStatus == UNAuthorizationStatusNotDetermined ||
          settings.authorizationStatus == UNAuthorizationStatusProvisional) {
        [PushNotificationUtil enableProvisionalPushNotificationPermission:^(
                                  BOOL granted, NSError* error) {
          if (granted && !error) {
            dispatch_async(dispatch_get_main_queue(), ^{
              PushNotificationService* service =
                  GetApplicationContext()->GetPushNotificationService();
              id<SystemIdentity> identity = authService->GetPrimaryIdentity(
                  signin::ConsentLevel::kSignin);
              for (PushNotificationClientId clientId : clientIds) {
                service->SetPreference(identity.gaiaID, clientId,
                                       clientEnabledForProvisional);
                if (clientId == PushNotificationClientId::kSendTab &&
                    deviceInfoSyncService) {
                  deviceInfoSyncService->RefreshLocalDeviceInfo();
                }
              }
            });
          }
        }];
        return;
      }
    }];
  }
}

@end
