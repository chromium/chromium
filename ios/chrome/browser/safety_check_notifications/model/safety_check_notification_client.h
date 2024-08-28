// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_MODEL_SAFETY_CHECK_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_MODEL_SAFETY_CHECK_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "base/functional/callback_forward.h"
#import "base/memory/scoped_refptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"

// A push notification client for managing Safety Check-related notifications.
// Observes Safety Check state changes to ensure notifications are accurate, and
// handles user registration, notification delivery, and user interaction.
class SafetyCheckNotificationClient
    : public PushNotificationClient,
      public IOSChromeSafetyCheckManagerObserver {
 public:
  explicit SafetyCheckNotificationClient(
      const scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~SafetyCheckNotificationClient() override;

  // `PushNotificationClient` overrides.
  void HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
  void OnSceneActiveForegroundBrowserReady() override;

  // Called when the scene becomes "active foreground" and the browser is
  // ready. `completion` will be called when all async operations are done.
  void OnSceneActiveForegroundBrowserReady(base::OnceClosure completion);

  // `IOSChromeSafetyCheckManagerObserver` overrides.
  void PasswordCheckStateChanged(PasswordSafetyCheckState state) override;
  void SafeBrowsingCheckStateChanged(
      SafeBrowsingSafetyCheckState state) override;
  void UpdateChromeCheckStateChanged(
      UpdateChromeSafetyCheckState state) override;
  void RunningStateChanged(RunningSafetyCheckState state) override;
  void ManagerWillShutdown(
      IOSChromeSafetyCheckManager* safety_check_manager) override;

 private:
  // Callback type used with `GetPendingRequest()`.
  using GetPendingRequestCallback =
      base::OnceCallback<void(UNNotificationRequest*)>;

  // Calls `completion` with a pending request matching `notification_id` if
  // there is one, or `nil` if there isn't one.
  void GetPendingRequest(NSString* notification_id,
                         GetPendingRequestCallback completion);

  // Returns true if the user has enabled Safety Check notifications, either in
  // the Notifications Settings UI or through an opt-in prompt (e.g., Magic
  // Stack, Safety Check page, Password Checkup page).
  bool IsPermitted();

  // Called when the notification request matching `notification_id` is cleared
  // from the pending notification requests schedule.
  void OnNotificationCleared(NSString* notification_id,
                             UNNotificationRequest* request);

  // Clears any previously scheduled notification(s) that match
  // `notification_id`. Runs `completion` at the end, once all async operations
  // have completed.
  void ClearNotification(NSString* notification_id,
                         base::OnceClosure completion);

  // Schedules a new Safe Browsing notification reflecting `state`, if
  // permitted. Runs `completion` at the end, once all async operations have
  // completed.
  void ScheduleSafeBrowsingNotification(SafeBrowsingSafetyCheckState state,
                                        base::OnceClosure completion);

  // Clears any existing Safe Browsing notification and schedules a new one
  // reflecting the latest `state`, if permitted. Runs `completion`
  // at the end, once all async operations have completed.
  void ClearAndRescheduleSafeBrowsingNotification(
      SafeBrowsingSafetyCheckState state,
      base::OnceClosure completion);

  // Validates asynchronous `PushNotificationClient` events are evaluated on the
  // same sequence that `SafetyCheckNotificationClient` was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Ensures `IOSChromeSafetyCheckManagerObserver` events are posted on the
  // same sequence that `SafetyCheckNotificationClient` was created on.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<SafetyCheckNotificationClient> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_MODEL_SAFETY_CHECK_NOTIFICATION_CLIENT_H_
