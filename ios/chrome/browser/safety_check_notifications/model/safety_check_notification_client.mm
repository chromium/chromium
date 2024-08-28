// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check_notifications/model/safety_check_notification_client.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/task/bind_post_task.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/browser/safety_check_notifications/utils/utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace {

// Returns the first notification from `requests` whose identifier matches
// `identifier`.
UNNotificationRequest* NotificationWithIdentifier(
    NSString* identifier,
    NSArray<UNNotificationRequest*>* requests) {
  for (UNNotificationRequest* request in requests) {
    if ([request.identifier isEqualToString:identifier]) {
      return request;
    }
  }

  return nil;
}

}  // namespace

SafetyCheckNotificationClient::SafetyCheckNotificationClient(
    const scoped_refptr<base::SequencedTaskRunner> task_runner)
    : PushNotificationClient(PushNotificationClientId::kSafetyCheck),
      task_runner_(task_runner) {
  CHECK(task_runner);
}

SafetyCheckNotificationClient::~SafetyCheckNotificationClient() = default;

void SafetyCheckNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/356624000): Implement `HandleNotificationInteraction()` to
  // process user interactions with notifications (e.g., taps, dismissals).
}

UIBackgroundFetchResult
SafetyCheckNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/356626805): Implement `HandleNotificationReception()` to log
  // notification receipt for analytics/debugging.

  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
SafetyCheckNotificationClient::RegisterActionableNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/356624328): Implement actionable Safety Check notifications
  // allowing users to take direct actions from the notification.

  return @[];
}

void SafetyCheckNotificationClient::OnSceneActiveForegroundBrowserReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OnSceneActiveForegroundBrowserReady(base::DoNothing());
}

void SafetyCheckNotificationClient::OnSceneActiveForegroundBrowserReady(
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/362479882): Exit if user shouldn't receive a new Safe
  // Browsing notification (e.g., notifications disabled, recent notification
  // already shown).
  if (!IsPermitted()) {
    std::move(completion).Run();
    return;
  }

  // Confirm that `SafetyCheckNotificationClient` is not observing
  // `IOSChromeSafetyCheckManager` before registering itself as an observer for
  // Safety Check updates.
  if (!IOSChromeSafetyCheckManagerObserver::IsInObserverList()) {
    Browser* browser = GetSceneLevelForegroundActiveBrowser();

    if (!browser) {
      std::move(completion).Run();
      return;
    }

    ChromeBrowserState* browser_state = browser->GetBrowserState();

    IOSChromeSafetyCheckManager* safety_check_manager =
        IOSChromeSafetyCheckManagerFactory::GetForBrowserState(browser_state);

    safety_check_manager->AddObserver(this);

    ClearAndRescheduleSafeBrowsingNotification(
        safety_check_manager->GetSafeBrowsingCheckState(),
        std::move(completion));

    return;
  }

  // TODO(crbug.com/347975105): Implement
  // `OnSceneActiveForegroundBrowserReady()` to conditionally schedule
  // notifications.
  std::move(completion).Run();
}

void SafetyCheckNotificationClient::PasswordCheckStateChanged(
    PasswordSafetyCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/362486324): Re-schedule Safety Check notifications when the
  // Passwords state changes.
}

void SafetyCheckNotificationClient::SafeBrowsingCheckStateChanged(
    SafeBrowsingSafetyCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Avoid modifying notifications while the Safe Browsing check is running.
  // Wait for a meaningful state change that influences whether Safe Browsing
  // notifications should be removed or scheduled.
  if (state == SafeBrowsingSafetyCheckState::kRunning) {
    return;
  }

  ClearAndRescheduleSafeBrowsingNotification(state, base::DoNothing());
}

void SafetyCheckNotificationClient::UpdateChromeCheckStateChanged(
    UpdateChromeSafetyCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/362487375): Re-schedule Safety Check notifications when the
  // Update Chrome state changes.
}

void SafetyCheckNotificationClient::RunningStateChanged(
    RunningSafetyCheckState state) {
  // Do nothing. This method is currently a no-op as the running state of Safety
  // Check does not directly impact notification scheduling or removal.
}

void SafetyCheckNotificationClient::ManagerWillShutdown(
    IOSChromeSafetyCheckManager* safety_check_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  safety_check_manager->RemoveObserver(this);
}

void SafetyCheckNotificationClient::GetPendingRequest(
    NSString* notification_id,
    GetPendingRequestCallback completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto callback = base::CallbackToBlock(base::BindPostTask(
      task_runner_, base::BindOnce(&NotificationWithIdentifier, notification_id)
                        .Then(std::move(completion))));

  [UNUserNotificationCenter.currentNotificationCenter
      getPendingNotificationRequestsWithCompletionHandler:callback];
}

bool SafetyCheckNotificationClient::IsPermitted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/362260014): Replace current opt-in state logic with
  // `GetMobileNotificationPermissionStatusForClient()` once
  // `PushNotificationClient` dependencies are refactored.
  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();

  return local_pref_service
      ->GetDict(prefs::kAppLevelPushNotificationPermissions)
      .FindBool(kSafetyCheckNotificationKey)
      .value_or(false);
}

void SafetyCheckNotificationClient::OnNotificationCleared(
    NSString* notification_id,
    UNNotificationRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!request) {
    // TODO(crbug.com/362481419): Add logging to track the state of the
    // notification (requested, triggered, etc.).
    return;
  }

  [UNUserNotificationCenter.currentNotificationCenter
      removePendingNotificationRequestsWithIdentifiers:@[ notification_id ]];
}

void SafetyCheckNotificationClient::ClearNotification(
    NSString* notification_id,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetPendingRequest(
      notification_id,
      base::BindOnce(&SafetyCheckNotificationClient::OnNotificationCleared,
                     weak_ptr_factory_.GetWeakPtr(), notification_id)
          .Then(std::move(completion)));
}

void SafetyCheckNotificationClient::ScheduleSafeBrowsingNotification(
    SafeBrowsingSafetyCheckState state,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/362479882): Exit if user shouldn't receive a new Safe
  // Browsing notification (e.g., notifications disabled, recent notification
  // already shown).
  if (!IsPermitted()) {
    std::move(completion).Run();
    return;
  }

  // TODO(crbug.com/362481419): Add completion handler to log metrics and
  // actions when the Safe Browsing notification is requested.

  UNNotificationRequest* safe_browsing_notification =
      SafeBrowsingNotificationRequest(state);

  if (safe_browsing_notification) {
    [UNUserNotificationCenter.currentNotificationCenter
        addNotificationRequest:safe_browsing_notification
         withCompletionHandler:nil];
  }

  std::move(completion).Run();
}

void SafetyCheckNotificationClient::ClearAndRescheduleSafeBrowsingNotification(
    SafeBrowsingSafetyCheckState state,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ClearNotification(
      kSafetyCheckSafeBrowsingNotificationID,
      base::BindOnce(
          &SafetyCheckNotificationClient::ScheduleSafeBrowsingNotification,
          weak_ptr_factory_.GetWeakPtr(), state, std::move(completion)));
}
