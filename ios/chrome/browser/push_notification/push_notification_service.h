// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_H_

#import <memory>

#import "ios/chrome/browser/push_notification/push_notification_configuration.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs
class PushNotificationClientManager;

// Service responsible for establishing connection and interacting
// with the push notification server.
class PushNotificationService {
 public:
  using CompletionHandler = void (^)(NSError* error);
  using PreferenceMap = NSDictionary<NSString*, NSNumber*>*;

  PushNotificationService();
  virtual ~PushNotificationService();

  // Initializes the device's connection and registers it to the push
  // notification server. `completion_handler` is invoked asynchronously when
  // the operation successfully or unsuccessfully completes.
  virtual void RegisterDevice(PushNotificationConfiguration* config,
                              CompletionHandler completion_handler) = 0;

  // Disassociates the device to its previously associated accounts on the push
  // notification server. `completion_handler` is invoked asynchronously when
  // the operation successfully or unsuccessfully completes.
  virtual void UnregisterDevice(CompletionHandler completion_handler) = 0;

  // Updates the current user's push notification preferences with the push
  // notification server.
  void UpdateFeaturePushNotificationPreferences(
      NSString* account_id,
      PreferenceMap preference_map,
      CompletionHandler completion_handler) {}

  // Registers each PushNotificationClient's prefs. Each
  // PushNotificationClient's ability to send push notifications to the user is
  // disabled by default.
  static void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry);

  // Returns PushNotificationService's PushNotificationClientManager.
  PushNotificationClientManager* GetPushNotificationClientManager();

 private:
  // The PushNotificationClientManager manages all interactions between the
  // system and push notification enabled features.
  std::unique_ptr<PushNotificationClientManager> client_manager_;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_H_
