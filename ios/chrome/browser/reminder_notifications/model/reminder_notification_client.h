// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import <memory>
#import <optional>

#import "base/sequence_checker.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

class PrefChangeRegistrar;
class ProfileIOS;

// A Profile-based notification client responsible for scheduling reminder
// notification requests and handling user interactions with reminders.
class ReminderNotificationClient : public PushNotificationClient {
 public:
  explicit ReminderNotificationClient(ProfileIOS* profile);
  ~ReminderNotificationClient() override;

  // Override PushNotificationClient::
  std::optional<NotificationType> GetNotificationType(
      UNNotification* notification) override;
  bool CanHandleNotification(UNNotification* notification) override;
  bool HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;

 private:
  // Returns true if the client is permitted to schedule notifications.
  bool IsPermitted();

  // Called when the relevant Reminder Notifications Pref changes.
  void OnReminderDataPrefChanged();

  // Called when the relevant permissions Pref changes.
  void OnPermissionsPrefChanged();

  // Schedules new reminder notifications based on the current reminder data in
  // Prefs.
  void ScheduleNewReminders();

  // Schedules new reminder notifications if they don't already exist in the
  // notification center.
  void ScheduleNewRemindersIfNeeded(
      NSArray<UNNotificationRequest*>* pending_requests);

  // Cancels all pending reminder notifications.
  void CancelAllNotifications(base::OnceClosure completion_handler);

  // Callback for `-getPendingNotificationRequestsWithCompletionHandler:` used
  // in `CancelAllNotifications()`.
  void OnGetPendingNotificationsForCancellation(
      base::OnceClosure completion_handler,
      NSArray<UNNotificationRequest*>* requests);

  // Schedules a single reminder notification for `reminder_url` using
  // `reminder_details`.
  void ScheduleNotification(const GURL& reminder_url,
                            const base::Value::Dict& reminder_details,
                            std::string_view profile_name);

  // Called upon completion of scheduling a single notification. Removes the
  // corresponding Pref entry for `scheduled_url` if scheduling was successful.
  void OnNotificationScheduled(const GURL& scheduled_url, NSError* error);

  // Used to assert that asynchronous callback are invoked on the correct
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Observes Pref changes for the Profile associated with this client.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::WeakPtrFactory<ReminderNotificationClient> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_CLIENT_H_
