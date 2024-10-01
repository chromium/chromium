// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_util.h"

#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace {

using PermissionResponseHandler = void (^)(BOOL granted,
                                           BOOL promptedUser,
                                           NSError* error);
using push_notification::SettingsAuthorizationStatus;

using ProvisionalPermissionResponseHandler = void (^)(BOOL granted,
                                                      NSError* error);

// This enum is used to record the action a user performed when prompted to
// allow push notification permissions.
enum class PermissionPromptAction {
  ACCEPTED,
  DECLINED,
  ERROR,
  kMaxValue = ERROR
};

enum class ProvisionalPermissionAction {
  ENABLED,
  INELIGIBLE,
  ERROR,
  kMaxValue = ERROR
};

// The histogram used to record the outcome of the permission prompt.
const char kEnabledPermissionsHistogram[] =
    "IOS.PushNotification.EnabledPermisisons";

// The histogram used to record the outcome of the provisional notifications
// permission.
const char kProvisionalEnabledPermissionsHistogram[] =
    "IOS.PushNotification.Provisional.EnabledPermissions";

// The histogram used to record the user's push notification authorization
// status.
const char kAuthorizationStatusHistogram[] =
    "IOS.PushNotification.NotificationSettingsAuthorizationStatus";

// The histogram used to record users changes to an authorized push notification
// permission status.
const char kNotificationAutorizationStatusChangedToAuthorized[] =
    "IOS.PushNotification.NotificationAutorizationStatusChangedToAuthorized";

// The histogram used to record users changes to a denied push notification
// permission status.
const char kNotificationAutorizationStatusChangedToDenied[] =
    "IOS.PushNotification.NotificationAutorizationStatusChangedToDenied";

// The histogram used to record users changes to a provisional push notification
// permission status.
const char kNotificationAutorizationStatusChangedToProvisional[] =
    "IOS.PushNotification.NotificationAutorizationStatusChangedToProvisional";

// Key for the pre-rendered payload from Chime.
NSString* const kPrerenderedPayloadKey = @"$";

// Key for the client id in the payload.
NSString* const kClientIdFieldKey = @"n";

// The options to use when requestion notification authorization.
const UNAuthorizationOptions kAuthorizationOptions =
    UNAuthorizationOptionAlert | UNAuthorizationOptionBadge |
    UNAuthorizationOptionSound;

}  // namespace

@implementation PushNotificationUtil

+ (void)registerDeviceWithAPNSWithProvisionalNotificationsAvailable:
    (BOOL)provisionalNotificationsAvailable {
  [PushNotificationUtil
      getPermissionSettings:^(UNNotificationSettings* settings) {
        // Logs the users iOS settings' push notification permission status over
        // time.
        [PushNotificationUtil
            logPermissionSettingsMetrics:settings.authorizationStatus];
        if (settings.authorizationStatus == UNAuthorizationStatusAuthorized ||
            provisionalNotificationsAvailable) {
          [[UIApplication sharedApplication] registerForRemoteNotifications];
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

+ (void)enableProvisionalPushNotificationPermission:
    (ProvisionalPermissionResponseHandler)completionHandler {
  [PushNotificationUtil
      getPermissionSettings:^(UNNotificationSettings* settings) {
        [PushNotificationUtil
            enableProvisionalPushNotificationPermission:completionHandler
                                     permissionSettings:settings];
      }];
}

+ (void)getPermissionSettings:
    (void (^)(UNNotificationSettings* settings))completionHandler {
  UNUserNotificationCenter* center =
      UNUserNotificationCenter.currentNotificationCenter;
  if (!web::WebThread::IsThreadInitialized(web::WebThread::UI)) {
    // In some circumstances, like when the application is going through a cold
    // startup, this function is called before Chrome threads have been
    // initialized. In this case, the function relies on native infrastructure
    // to schedule and execute the callback on the main thread.
    void (^permissionHandler)(UNNotificationSettings*) =
        ^(UNNotificationSettings* settings) {
          dispatch_async(dispatch_get_main_queue(), ^{
            completionHandler(settings);
          });
        };

    [center getNotificationSettingsWithCompletionHandler:permissionHandler];
    return;
  }

  scoped_refptr<base::SequencedTaskRunner> thread;
  // To avoid unnecessarily posting callbacks to the UI thread, the current
  // thread is used if it is suitable for callback execution.
  if (base::SequencedTaskRunner::HasCurrentDefault()) {
    thread = base::SequencedTaskRunner::GetCurrentDefault();
  } else {
    thread = web::GetUIThreadTaskRunner({});
  }

  void (^permissionHandler)(UNNotificationSettings*) =
      ^(UNNotificationSettings* settings) {
        thread->PostTask(FROM_HERE, base::BindOnce(^{
                           completionHandler(settings);
                         }));
      };

  [center getNotificationSettingsWithCompletionHandler:permissionHandler];
}

// This function returns the value stored in the prefService that represents the
// user's iOS settings permission status for push notifications.
+ (UNAuthorizationStatus)getSavedPermissionSettings {
  ApplicationContext* context = GetApplicationContext();
  PrefService* prefService = context->GetLocalState();
  int previousStatus =
      prefService->GetInteger(prefs::kPushNotificationAuthorizationStatus);
  switch (previousStatus) {
    case (int)SettingsAuthorizationStatus::NOTDETERMINED:
      return UNAuthorizationStatusNotDetermined;
    case (int)SettingsAuthorizationStatus::DENIED:
      return UNAuthorizationStatusDenied;
    case (int)SettingsAuthorizationStatus::AUTHORIZED:
      return UNAuthorizationStatusAuthorized;
    case (int)SettingsAuthorizationStatus::PROVISIONAL:
      return UNAuthorizationStatusProvisional;
    case (int)SettingsAuthorizationStatus::EPHEMERAL:
      return UNAuthorizationStatusEphemeral;
    default:
      return UNAuthorizationStatusNotDetermined;
  }
}

+ (void)updateAuthorizationStatusPref {
  [PushNotificationUtil
      getPermissionSettings:^(UNNotificationSettings* settings) {
        [PushNotificationUtil
            updateAuthorizationStatusPref:settings.authorizationStatus];
      }];
}

+ (void)updateAuthorizationStatusPref:(UNAuthorizationStatus)status {
  ApplicationContext* context = GetApplicationContext();
  PrefService* prefService = context->GetLocalState();
  SettingsAuthorizationStatus previousStatus =
      static_cast<SettingsAuthorizationStatus>(
          prefService->GetInteger(prefs::kPushNotificationAuthorizationStatus));
  BOOL changeWasLogged = [PushNotificationUtil
      logChangeInAuthorizationStatusFrom:previousStatus
                                      to:[PushNotificationUtil
                                             getNotificationSettingsStatusFrom:
                                                 status]];
  if (changeWasLogged) {
    prefService->SetInteger(prefs::kPushNotificationAuthorizationStatus,
                            base::to_underlying(status));
  }
}

// Converts an UNAuthorizationStatus enum to a
// push_notification::SettingsAuthorizationStatus enum.
+ (SettingsAuthorizationStatus)getNotificationSettingsStatusFrom:
    (UNAuthorizationStatus)status {
  switch (status) {
    case UNAuthorizationStatusNotDetermined:
      // The authorization status is this case when the user has not yet
      // decided to give Chrome push notification permissions.
      return SettingsAuthorizationStatus::NOTDETERMINED;
    case UNAuthorizationStatusDenied:
      // The authorization status is this case when the user has denied to
      // give Chrome push notification permissions via the push
      // notification iOS system permission prompt or by navigating to the iOS
      // settings and manually enabling it.
      return SettingsAuthorizationStatus::DENIED;
    case UNAuthorizationStatusAuthorized:
      // The authorization status is this case when the user has
      // authorized to give Chrome push notification permissions via the
      // push notification iOS system permission prompt or by navigating to the
      // iOS settings and manually enabling it.
      return SettingsAuthorizationStatus::AUTHORIZED;
    case UNAuthorizationStatusProvisional:
      // The authorization status is this case when Chrome has the ability
      // to send provisional push notifications.
      return SettingsAuthorizationStatus::PROVISIONAL;
    case UNAuthorizationStatusEphemeral:
      // The authorization status is this case Chrome can receive
      // notifications for a limited amount of time.
      return SettingsAuthorizationStatus::EPHEMERAL;
  }
}

+ (std::optional<PushNotificationClientId>)
    mapToPushNotificationClientIdFromUserInfo:
        (NSDictionary<NSString*, id>*)userInfo {
  // The client mapping rubric for mapping chime ids to Push Notification Client
  // Ids. Sports maps to Content.
  NSDictionary<NSString*, NSNumber*>* clientIdMappings = @{
    @"commerce_price_drop" : [NSNumber
        numberWithInt:static_cast<int>(PushNotificationClientId::kCommerce)],
    @"content_push_notify" : [NSNumber
        numberWithInt:static_cast<int>(PushNotificationClientId::kContent)],
    @"sports_push_notify" : [NSNumber
        numberWithInt:static_cast<int>(PushNotificationClientId::kContent)],
    @"send_tab_notify" : [NSNumber
        numberWithInt:static_cast<int>(PushNotificationClientId::kSendTab)],
  };

  NSString* payloadText = userInfo[kPrerenderedPayloadKey][kClientIdFieldKey];
  if (payloadText.length) {
    // Removes the unstable prefix from the chime client id.
    NSString* resultingClient =
        [[payloadText componentsSeparatedByString:@":"][1]
            stringByReplacingOccurrencesOfString:@"_unstable"
                                      withString:@""];
    NSNumber* number = clientIdMappings[resultingClient];
    if (number) {
      return static_cast<PushNotificationClientId>(number.intValue);
    }
  }
  return std::nullopt;
}

#pragma mark - Private

// Displays the push notification permission prompt if the user has not decided
// on the application's permission status.
+ (void)requestPushNotificationPermission:(PermissionResponseHandler)completion
                       permissionSettings:(UNNotificationSettings*)settings {
  if (![self canPromptForAuthorization:settings]) {
    if (completion) {
      completion(
          settings.authorizationStatus == UNAuthorizationStatusAuthorized, NO,
          nil);
    }
    return;
  }
  UNUserNotificationCenter* center =
      UNUserNotificationCenter.currentNotificationCenter;
  [center requestAuthorizationWithOptions:kAuthorizationOptions
                        completionHandler:^(BOOL granted, NSError* error) {
                          [PushNotificationUtil
                              requestAuthorizationResult:completion
                                                 granted:granted
                                                   error:error];
                        }];
}

// Enrolls the user in provisional notifications.
+ (void)enableProvisionalPushNotificationPermission:
            (ProvisionalPermissionResponseHandler)completion
                                 permissionSettings:
                                     (UNNotificationSettings*)settings {
  if (settings.authorizationStatus != UNAuthorizationStatusNotDetermined) {
    if (completion) {
      completion(
          settings.authorizationStatus == UNAuthorizationStatusProvisional,
          nil);
    }
    base::UmaHistogramEnumeration(kProvisionalEnabledPermissionsHistogram,
                                  ProvisionalPermissionAction::INELIGIBLE);
    return;
  }
  UNAuthorizationOptions options =
      kAuthorizationOptions | UNAuthorizationOptionProvisional;
  UNUserNotificationCenter* center =
      UNUserNotificationCenter.currentNotificationCenter;
  [center requestAuthorizationWithOptions:options
                        completionHandler:^(BOOL granted, NSError* error) {
                          [PushNotificationUtil
                              requestProvisionalAuthorizationResult:completion
                                                            granted:granted
                                                              error:error];
                        }];
}

// Reports the push notification permission prompt's outcome to metrics.
+ (void)requestAuthorizationResult:(PermissionResponseHandler)completion
                           granted:(BOOL)granted
                             error:(NSError*)error {
  if (granted) {
    [PushNotificationUtil
        registerDeviceWithAPNSWithProvisionalNotificationsAvailable:NO];
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
  [PushNotificationUtil updateAuthorizationStatusPref];
}

// Reports the push notification permission prompt's outcome to metrics and
// registers the device to APNs.
+ (void)requestProvisionalAuthorizationResult:
            (ProvisionalPermissionResponseHandler)completion
                                      granted:(BOOL)granted
                                        error:(NSError*)error {
  if (granted) {
    [PushNotificationUtil
        registerDeviceWithAPNSWithProvisionalNotificationsAvailable:NO];
    base::UmaHistogramEnumeration(kProvisionalEnabledPermissionsHistogram,
                                  ProvisionalPermissionAction::ENABLED);
  } else if (!granted || error) {
    base::UmaHistogramEnumeration(kProvisionalEnabledPermissionsHistogram,
                                  ProvisionalPermissionAction::ERROR);
  }

  if (completion) {
    completion(granted, error);
  }
  [PushNotificationUtil updateAuthorizationStatusPref];
}

// Logs the permission status, stored in iOS settings, the user has given for
// whether Chrome can receive push notifications on the device to UMA.
+ (void)logPermissionSettingsMetrics:
    (UNAuthorizationStatus)authorizationStatus {
  SettingsAuthorizationStatus status = [PushNotificationUtil
      getNotificationSettingsStatusFrom:authorizationStatus];
  base::UmaHistogramEnumeration(kAuthorizationStatusHistogram, status);
}

// This function logs the `previousStatus` to UMA if the push notificaiton
// authorization status that is stored in the prefService is differnet from the
// authorization status currently set on the user's device. The function returns
// YES if the function logs to UMA. Otherwise, it returns NO.
+ (BOOL)logChangeInAuthorizationStatusFrom:
            (SettingsAuthorizationStatus)previousStatus
                                        to:(SettingsAuthorizationStatus)status {
  if (previousStatus == status) {
    return NO;
  }

  if (status == SettingsAuthorizationStatus::AUTHORIZED) {
    base::UmaHistogramEnumeration(
        kNotificationAutorizationStatusChangedToAuthorized, previousStatus);
    return YES;
  }

  if (status == SettingsAuthorizationStatus::DENIED) {
    base::UmaHistogramEnumeration(
        kNotificationAutorizationStatusChangedToDenied, previousStatus);
    return YES;
  }

  if (status == SettingsAuthorizationStatus::PROVISIONAL) {
    base::UmaHistogramEnumeration(
        kNotificationAutorizationStatusChangedToProvisional, previousStatus);
    return YES;
  }

  return NO;
}

// Returns YES if the user can be prompted for notification authorization.
+ (BOOL)canPromptForAuthorization:(UNNotificationSettings*)settings {
  switch (settings.authorizationStatus) {
    case UNAuthorizationStatusNotDetermined:
    case UNAuthorizationStatusProvisional:
      return YES;
    case UNAuthorizationStatusDenied:
    case UNAuthorizationStatusAuthorized:
      return NO;
    case UNAuthorizationStatusEphemeral:
      // This authorization status only applies to app clips.
      return NO;
  }
}

@end
