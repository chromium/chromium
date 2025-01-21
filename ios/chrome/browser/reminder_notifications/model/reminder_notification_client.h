// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import <optional>

#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"

class PrefChangeRegistrar;

// A notification client responsible for scheduling reminder notification
// requests and handling user interactions with reminders.
class ReminderNotificationClient : public PushNotificationClient,
                                   public ProfileManagerObserverIOS {
 public:
  ReminderNotificationClient(ProfileManagerIOS* profile_manager);
  ~ReminderNotificationClient() override;

  // Override PushNotificationClient::
  bool HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
  void OnSceneActiveForegroundBrowserReady() override;

  // Called when the scene becomes "active foreground" and the browser is
  // ready. The closure will be called when all async operations are done.
  void OnSceneActiveForegroundBrowserReady(base::OnceClosure closure);

  // ProfileManagerObserverIOS:
  void OnProfileManagerDestroyed(ProfileManagerIOS* manager) override;
  void OnProfileCreated(ProfileManagerIOS* manager,
                        ProfileIOS* profile) override;
  void OnProfileLoaded(ProfileManagerIOS* manager,
                       ProfileIOS* profile) override;
  void OnProfileUnloaded(ProfileManagerIOS* manager,
                         ProfileIOS* profile) override;
  void OnProfileMarkedForPermanentDeletion(ProfileManagerIOS* manager,
                                           ProfileIOS* profile) override;

 private:
  // Observe pref changes for a profile.
  void ObserveProfilePrefs(ProfileIOS* profile);
  // Stop observing pref changes for a profile.
  void StopObservingProfilePrefs(ProfileIOS* profile);
  // Called when prefs change for a profile.
  void OnProfilePrefsChanged(ProfileIOS* profile);

  // Used to assert that asynchronous callback are invoked on the correct
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Observation of the ProfileManagerIOS.
  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      profile_manager_observation_{this};

  // Observations of ProfilePrefs for each loaded profile.
  base::flat_map<ProfileIOS*, std::unique_ptr<PrefChangeRegistrar>>
      pref_observers_;

  base::WeakPtrFactory<ReminderNotificationClient> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_CLIENT_H_
