// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_SERVICE_H_

#import <Foundation/Foundation.h>

#import <memory>

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefRegistrySimple;

@class PushNotificationAccountContextManager;
enum class PushNotificationClientId;
class PushNotificationClientManager;
@class PushNotificationConfiguration;

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

  // Returns whether the device has retrieved and stored its APNS device token.
  virtual bool DeviceTokenIsSet() const = 0;

  // Returns the representative target ID for the given Gaia ID. Returns null if
  // the user with the associated gaia_id is not registered for push
  // notifications.
  virtual std::string GetRepresentativeTargetIdForGaiaId(NSString* gaia_id);

  // Returns PushNotificationService's PushNotificationClientManager.
  PushNotificationClientManager* GetPushNotificationClientManager();

  // Returns PushNotificationService's PushNotificationAccountContextManager.
  PushNotificationAccountContextManager* GetAccountContextManager();

  // Enables/disables `client_id` ability to send push notifications to the
  // primary account signed into Chrome across synced iOS devices.
  void SetPreference(NSString* account_id,
                     PushNotificationClientId client_id,
                     bool enabled);

  // Registers the new account to the push notification server. In a multi
  // Profile environment, the PushNotificationService tracks the signed in
  // account across Profiles.
  void RegisterAccount(NSString* account_id,
                       CompletionHandler completion_handler);

  // Unregisters the account from the push notification server. In a multi
  // Profile environment, the account will not be signed out until it's
  // signed out across Profiles.
  void UnregisterAccount(NSString* account_id,
                         CompletionHandler completion_handler);

  // Registers each PushNotificationClient's prefs. Each
  // PushNotificationClient's ability to send push notifications to the user is
  // disabled by default.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Registers the push notification settings status stored in the device's iOS
  // settings.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 protected:
  // Registers the device with the push notification server. By supplying a list
  // of the GAIA IDs currently logged into Chrome on the device and the device's
  // APNS token, the server associates the GAIA IDs to the device, which allows
  // the server to begin sending push notifications to that device. When this
  // method is called, the server overwrites the accountIDs that are associated
  // with the device token with the given array of accountIDs. Thus, to
  // unregister an account from receiving push notifications on the device, this
  // method should be called with an array of accountIDs that omits the account
  // that is intended to be unregistered.
  virtual void SetAccountsToDevice(NSArray<NSString*>* account_ids,
                                   CompletionHandler completion_handler) {}

  // Updates the current user's push notification preferences with the push
  // notification server.
  virtual void SetPreferences(NSString* account_id,
                              PreferenceMap preference_map,
                              CompletionHandler completion_handler) = 0;

 private:
  // The PushNotificationClientManager manages all interactions between the
  // system and push notification enabled features.
  std::unique_ptr<PushNotificationClientManager> client_manager_;

  // Stores a mapping of each account's GAIA ID signed into the device to its
  // context object. This object contains the account's pref service values
  // pertaining to push notification supported features and the number of times
  // the given account is signed in across multiple profiles.
  __strong PushNotificationAccountContextManager* context_manager_;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_SERVICE_H_
