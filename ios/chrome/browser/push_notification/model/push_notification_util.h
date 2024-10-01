// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_UTIL_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_UTIL_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import <optional>

#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"

@class UIApplication;
@class UNNotificationCategory;
@class UNNotificationSettings;

namespace push_notification {

// Multiple UMA metrics and the local state pref service rely on this enum.
// Please do not reorder or delete its entries.
enum class SettingsAuthorizationStatus {
  NOTDETERMINED,
  DENIED,
  AUTHORIZED,
  PROVISIONAL,
  EPHEMERAL,
  kMaxValue = EPHEMERAL
};
}  // namespace push_notification

// This collection of class functions' purpose is to encapsulate the push
// notification functionality that must interact with Apple in some manner,
// either by interacting with iOS or Apple Push Notification Service (APNS).
@interface PushNotificationUtil : NSObject

// The function registers the device with APNS. AppDelegate's
// didRegisterForNotificationsWithDeviceToken function is called if the device
// was successfully registered with APNS. If the device's registration was
// unsuccessful, then AppDelegate's didRegisterForNotificationsWithError
// function is called. `provisionalNotificationsAvailable` is YES when a
// notification type that can deliver provisional notifications (Content or
// SendTab) notification is enabled or need to be registered.
+ (void)registerDeviceWithAPNSWithProvisionalNotificationsAvailable:
    (BOOL)provisionalNotificationsAvailable;

// The function registers the set of `UNNotificationCategory` objects with iOS'
// UNNotificationCenter.
+ (void)registerActionableNotifications:
    (NSSet<UNNotificationCategory*>*)categories;

// This function displays a permission request system prompt. On display of this
// prompt, the user must decide whether or not to allow iOS to notify them of
// incoming Chromium push notifications. If the user decides to allow push
// notifications, then `completionHandler` is executed with `granted` equaling
// `true`. Also, there is a possibility that `completionHandler` will be
// executed in a background thread. In addition, this function reports
// permission request's outcome to metrics. Since iOS only allows applications
// to prompt users for push notifications permissons once, the `promptShown`
// parameter captures whether the permission prompt was indeed displayed to the
// user. If the prompt is not displayed, then `granted` is equal to the user's
// iOS push notification permission setting for Chrome.
+ (void)requestPushNotificationPermission:
    (void (^)(BOOL granted, BOOL promptShown, NSError* error))completionHandler;

// This function enrolls the user into provisional notifications. If the OS
// grants this permission, then `completionHandler` is executed with `granted`
// equaling true. Also, there is a possibility that `completionHandler` will
// be executed in a background thread. In addition, this function reports
// permission request's outcome to metrics. It only grants permission if the
// notification authorization status is NotDetermined which indicates it was
// never set.  If notifications were already enabled, it returns `granted`
// equaling true only if the type enabled is Provisional, otherwise it returns
// false. This function does not present a prompt to the user and runs its
// logic silently.
+ (void)enableProvisionalPushNotificationPermission:
    (void (^)(BOOL granted, NSError* error))completionHandler;

// This functions retrieves the authorization and feature-related settings for
// push notifications. This function ensures that the `completionHandler` is
// executed on the application's main thread.
+ (void)getPermissionSettings:
    (void (^)(UNNotificationSettings* settings))completionHandler;

// This functions retrieves the currently saved authorization settings for
// push notifications. This is used for features that require knowledge of
// status changes on notification permissions.
+ (UNAuthorizationStatus)getSavedPermissionSettings;

// Gets the authorization status from iOS and updates prefs if needed.
+ (void)updateAuthorizationStatusPref;

// This function updates the value stored in the prefService that represents the
// user's iOS settings permission status for push notifications. If there is a
// difference between the prefService's previous value and the new value, the
// change is logged to UMA.
+ (void)updateAuthorizationStatusPref:(UNAuthorizationStatus)status;

// Returns the corresponding SettingsAuthorizationStatus value for the given
// `status`.
+ (push_notification::SettingsAuthorizationStatus)
    getNotificationSettingsStatusFrom:(UNAuthorizationStatus)status;

// This function maps a chime client id from a payload (`userInfo`) to a single
// PushNotificationClient.
+ (std::optional<PushNotificationClientId>)
    mapToPushNotificationClientIdFromUserInfo:
        (NSDictionary<NSString*, id>*)userInfo;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_UTIL_H_
