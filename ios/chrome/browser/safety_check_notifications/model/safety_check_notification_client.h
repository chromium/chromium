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
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"

class Browser;

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
  bool HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
  void OnSceneActiveForegroundBrowserReady() override;

  // Called when the scene becomes "active foreground" and the browser is
  // ready. `completion` will be called when all async operations are done.
  void OnSceneActiveForegroundBrowserReady(base::OnceClosure completion);

  // `IOSChromeSafetyCheckManagerObserver` overrides.
  void PasswordCheckStateChanged(PasswordSafetyCheckState state,
                                 password_manager::InsecurePasswordCounts
                                     insecure_password_counts) override;
  void SafeBrowsingCheckStateChanged(
      SafeBrowsingSafetyCheckState state) override;
  void UpdateChromeCheckStateChanged(
      UpdateChromeSafetyCheckState state) override;
  void RunningStateChanged(RunningSafetyCheckState state) override;
  void ManagerWillShutdown(
      IOSChromeSafetyCheckManager* safety_check_manager) override;

 private:
  // Callback type used with `GetPendingRequests()`.
  using GetPendingRequestsCallback =
      base::OnceCallback<void(NSArray<UNNotificationRequest*>*)>;

  // Calls `completion` with all pending requests matching `identifiers`.
  void GetPendingRequests(NSArray<NSString*>* identifiers,
                          GetPendingRequestsCallback completion);

  // Returns true if the user has enabled Safety Check notifications, either in
  // the Notifications Settings UI or through an opt-in prompt (e.g., Magic
  // Stack, Safety Check page, Password Checkup page).
  bool IsPermitted();

  // Returns `true` if there is a foreground active browser.
  bool IsSceneLevelForegroundActive();

  // Called when notifications matching `identifiers` are cleared from the
  // pending notification requests schedule.
  void OnNotificationsCleared(NSArray<NSString*>* identifiers,
                              NSArray<UNNotificationRequest*>* requests);

  // Clears any previously scheduled notifications matching `identifiers`. Runs
  // `completion` at the end, once all async operations have completed.
  void ClearNotifications(NSArray<NSString*>* identifiers,
                          base::OnceClosure completion);

  // Schedules new Safety Check notifications reflecting `update_chrome_state`,
  // `safe_browsing_state`, and `password_state`/`insecure_password_counts`, if
  // permitted. Runs `completion` at the end, once all async operations have
  // completed.
  void ScheduleSafetyCheckNotifications(
      UpdateChromeSafetyCheckState update_chrome_state,
      SafeBrowsingSafetyCheckState safe_browsing_state,
      PasswordSafetyCheckState password_state,
      password_manager::InsecurePasswordCounts insecure_password_counts,
      base::OnceClosure completion);

  // Clears any existing Safety Check notifications and schedules new ones
  // reflecting the latest `update_chrome_state`, `safe_browsing_state`, and
  // `password_state`/`insecure_password_counts`, if permitted. Runs
  // `completion` at the end, once all async operations have completed.
  void ClearAndRescheduleSafetyCheckNotifications(
      UpdateChromeSafetyCheckState update_chrome_state,
      SafeBrowsingSafetyCheckState safe_browsing_state,
      PasswordSafetyCheckState password_state,
      password_manager::InsecurePasswordCounts insecure_password_counts,
      base::OnceClosure completion);

  // Navigates to and displays the relevant UI based on the provided
  // `notification_metadata`.
  void ShowUIForNotificationMetadata(NSDictionary* notification_metadata,
                                     Browser* browser);

  // Logs to a histogram if notifications that were requested have been
  // triggered.
  void LogTriggeredNotifications();

  // Logs to a histogram if notifications that were requested have been
  // dismissed.
  void LogDismissedNotifications();

  // Called with all the delivered `notifications` that are still present in
  // Notification Center.
  void OnGetDeliveredNotifications(NSArray<UNNotification*>* notifications);

  // Current state of the Update Chrome check.
  UpdateChromeSafetyCheckState update_chrome_check_state_ =
      UpdateChromeSafetyCheckState::kDefault;

  // Current state of the Password check.
  PasswordSafetyCheckState password_check_state_ =
      PasswordSafetyCheckState::kDefault;

  // Current state of the Safe Browsing check.
  SafeBrowsingSafetyCheckState safe_browsing_check_state_ =
      SafeBrowsingSafetyCheckState::kDefault;

  // The count of passwords flagged as compromised, dismissed, reused, and weak
  // by the Safety Check.
  password_manager::InsecurePasswordCounts insecure_password_counts_ = {
      /* compromised */ 0, /* dismissed */ 0, /* reused */ 0,
      /* weak */ 0};

  // When the user interacts with a Safety Check notification but there are no
  // foreground scenes, this will store notification metadata so it can
  // be handled when there is a foreground scene.
  NSDictionary* interacted_notification_metadata_;

  // Validates asynchronous `PushNotificationClient` events are evaluated on the
  // same sequence that `SafetyCheckNotificationClient` was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Ensures `IOSChromeSafetyCheckManagerObserver` events are posted on the
  // same sequence that `SafetyCheckNotificationClient` was created on.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<SafetyCheckNotificationClient> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_MODEL_SAFETY_CHECK_NOTIFICATION_CLIENT_H_
