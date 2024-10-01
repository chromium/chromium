// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check_notifications/model/safety_check_notification_client.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/bind_post_task.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/browser/safety_check_notifications/utils/utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"

namespace {

// Returns all notifications from `requests` matching `identifiers`, if any.
NSArray<UNNotificationRequest*>* NotificationsWithIdentifiers(
    NSSet<NSString*>* identifiers,
    NSArray<UNNotificationRequest*>* requests) {
  NSMutableArray<UNNotificationRequest*>* matching_requests =
      [NSMutableArray array];

  for (UNNotificationRequest* request in requests) {
    if ([identifiers containsObject:request.identifier]) {
      [matching_requests addObject:request];
    }
  }

  return matching_requests;
}

// Returns `true` if provisional Safety Check notifications are allowed based
// on:
//  - The existence of a compromised password notification.
//  - The current notification authorization status (provisional or not yet
//  determined).
bool CanSendProvisionalNotifications(
    PasswordSafetyCheckState password_check_state,
    password_manager::InsecurePasswordCounts insecure_password_counts,
    PrefService* local_pref_service) {
  CHECK(local_pref_service);

  if (!ProvisionalSafetyCheckNotificationsEnabled()) {
    return false;
  }

  // Only send provisional notifications for compromised passwords.
  if (password_check_state !=
      PasswordSafetyCheckState::kUnmutedCompromisedPasswords) {
    return false;
  }

  UNNotificationContent* password_notification =
      NotificationForPasswordCheckState(password_check_state,
                                        insecure_password_counts);

  // Only send provisional notifications if a password notification actually
  // exists.
  if (password_notification == nil) {
    return false;
  }

  UNAuthorizationStatus auth_status =
      [PushNotificationUtil getSavedPermissionSettings];

  return auth_status == UNAuthorizationStatusProvisional ||
         auth_status == UNAuthorizationStatusNotDetermined;
}

}  // namespace

SafetyCheckNotificationClient::SafetyCheckNotificationClient(
    const scoped_refptr<base::SequencedTaskRunner> task_runner)
    : PushNotificationClient(PushNotificationClientId::kSafetyCheck),
      task_runner_(task_runner) {
  CHECK(task_runner);
}

SafetyCheckNotificationClient::~SafetyCheckNotificationClient() = default;

bool SafetyCheckNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<SafetyCheckNotificationType> notification_type =
      ParseSafetyCheckNotificationType(response.notification.request);

  if (!notification_type.has_value()) {
    return false;
  }

  // If the app is not yet foreground active, store metadata about the
  // notification to handle it later when the app becomes foreground active.
  interacted_notification_metadata_ =
      response.notification.request.content.userInfo;

  if (![interacted_notification_metadata_ count]) {
    base::UmaHistogramEnumeration("IOS.Notifications.SafetyCheck.Interaction",
                                  SafetyCheckNotificationType::kError);
    return false;
  }

  base::UmaHistogramEnumeration("IOS.Notifications.SafetyCheck.Interaction",
                                notification_type.value());

  if (IsSceneLevelForegroundActive()) {
    ClearAndRescheduleSafetyCheckNotifications(
        update_chrome_check_state_, safe_browsing_check_state_,
        password_check_state_, insecure_password_counts_, base::DoNothing());
  }

  return true;
}

std::optional<UIBackgroundFetchResult>
SafetyCheckNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/356626805): Implement `HandleNotificationReception()` to log
  // notification receipt for analytics/debugging.

  return std::nullopt;
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

  // Confirm that `SafetyCheckNotificationClient` is not observing
  // `IOSChromeSafetyCheckManager` before registering itself as an observer for
  // Safety Check updates.
  if (!IOSChromeSafetyCheckManagerObserver::IsInObserverList()) {
    Browser* browser = GetSceneLevelForegroundActiveBrowser();

    if (!browser) {
      std::move(completion).Run();
      return;
    }

    ProfileIOS* profile = browser->GetProfile();

    IOSChromeSafetyCheckManager* safety_check_manager =
        IOSChromeSafetyCheckManagerFactory::GetForProfile(profile);

    safety_check_manager->AddObserver(this);

    update_chrome_check_state_ =
        safety_check_manager->GetUpdateChromeCheckState();
    safe_browsing_check_state_ =
        safety_check_manager->GetSafeBrowsingCheckState();
    password_check_state_ = safety_check_manager->GetPasswordCheckState();
    insecure_password_counts_ =
        safety_check_manager->GetInsecurePasswordCounts();
  }

  // TODO(crbug.com/362479882): Exit if the user shouldn't receive a new Safety
  // Check notification (e.g., notifications disabled, recent notification
  // already shown).
  if (!IsPermitted()) {
    std::move(completion).Run();
    return;
  }

  ClearAndRescheduleSafetyCheckNotifications(
      update_chrome_check_state_, safe_browsing_check_state_,
      password_check_state_, insecure_password_counts_, std::move(completion));
}

void SafetyCheckNotificationClient::PasswordCheckStateChanged(
    PasswordSafetyCheckState state,
    password_manager::InsecurePasswordCounts insecure_password_counts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Avoid modifying notifications while the Password check is running.
  // Wait for a meaningful state change that influences whether Password
  // notifications should be removed or scheduled.
  if (state == PasswordSafetyCheckState::kRunning) {
    return;
  }

  password_check_state_ = state;
  insecure_password_counts_ = insecure_password_counts;

  ClearAndRescheduleSafetyCheckNotifications(
      update_chrome_check_state_, safe_browsing_check_state_,
      password_check_state_, insecure_password_counts_, base::DoNothing());
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

  safe_browsing_check_state_ = state;

  ClearAndRescheduleSafetyCheckNotifications(
      update_chrome_check_state_, safe_browsing_check_state_,
      password_check_state_, insecure_password_counts_, base::DoNothing());
}

void SafetyCheckNotificationClient::UpdateChromeCheckStateChanged(
    UpdateChromeSafetyCheckState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Avoid modifying notifications while the Update Chrome check is running.
  // Wait for a meaningful state change that influences whether Update Chrome
  // notifications should be removed or scheduled.
  if (state == UpdateChromeSafetyCheckState::kRunning) {
    return;
  }

  update_chrome_check_state_ = state;

  ClearAndRescheduleSafetyCheckNotifications(
      update_chrome_check_state_, safe_browsing_check_state_,
      password_check_state_, insecure_password_counts_, base::DoNothing());
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

void SafetyCheckNotificationClient::GetPendingRequests(
    NSArray<NSString*>* identifiers,
    GetPendingRequestsCallback completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto callback = base::CallbackToBlock(base::BindPostTask(
      task_runner_, base::BindOnce(&NotificationsWithIdentifiers,
                                   [NSSet setWithArray:identifiers])
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

  if (CanSendProvisionalNotifications(password_check_state_,
                                      insecure_password_counts_,
                                      local_pref_service)) {
    return true;
  }

  return local_pref_service
      ->GetDict(prefs::kAppLevelPushNotificationPermissions)
      .FindBool(kSafetyCheckNotificationKey)
      .value_or(false);
}

bool SafetyCheckNotificationClient::IsSceneLevelForegroundActive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetSceneLevelForegroundActiveBrowser() != nullptr;
}

void SafetyCheckNotificationClient::OnNotificationsCleared(
    NSArray<NSString*>* identifiers,
    NSArray<UNNotificationRequest*>* requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (![requests count]) {
    LogTriggeredNotifications();
    LogDismissedNotifications();

    interacted_notification_metadata_ = nil;

    return;
  }

  LogDismissedNotifications();

  interacted_notification_metadata_ = nil;

  [UNUserNotificationCenter.currentNotificationCenter
      removePendingNotificationRequestsWithIdentifiers:identifiers];
}

void SafetyCheckNotificationClient::ClearNotifications(
    NSArray<NSString*>* identifiers,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetPendingRequests(
      identifiers,
      base::BindOnce(&SafetyCheckNotificationClient::OnNotificationsCleared,
                     weak_ptr_factory_.GetWeakPtr(), identifiers)
          .Then(std::move(completion)));
}

void SafetyCheckNotificationClient::ScheduleSafetyCheckNotifications(
    UpdateChromeSafetyCheckState update_chrome_state,
    SafeBrowsingSafetyCheckState safe_browsing_state,
    PasswordSafetyCheckState password_state,
    password_manager::InsecurePasswordCounts insecure_password_counts,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/362479882): Exit if the user shouldn't receive a new Safety
  // Check notification (e.g., notifications disabled, recent notification
  // already shown).
  if (!IsPermitted()) {
    std::move(completion).Run();
    return;
  }

  // If `experimental_arm` is `kSuccinct`, only one notification can be
  // scheduled at a time. Otherwise, multiple notifications can be scheduled
  // concurrently.
  SafetyCheckNotificationsExperimentalArm experimental_arm =
      SafetyCheckNotificationsExperimentTypeEnabled();

  UNNotificationRequest* password_notification =
      PasswordNotificationRequest(password_state, insecure_password_counts);

  if (password_notification && AreSafetyCheckPasswordsNotificationsAllowed()) {
    [UNUserNotificationCenter.currentNotificationCenter
        addNotificationRequest:password_notification
         withCompletionHandler:nil];

    base::UmaHistogramEnumeration("IOS.Notifications.SafetyCheck.Requested",
                                  SafetyCheckNotificationType::kPasswords);

    // In the `kSuccinct` experiment arm, only one notification is allowed at a
    // time. Exit early after scheduling it.
    if (experimental_arm ==
        SafetyCheckNotificationsExperimentalArm::kSuccinct) {
      std::move(completion).Run();
      return;
    }
  }

  UNNotificationRequest* safe_browsing_notification =
      SafeBrowsingNotificationRequest(safe_browsing_state);

  if (safe_browsing_notification &&
      AreSafetyCheckSafeBrowsingNotificationsAllowed()) {
    [UNUserNotificationCenter.currentNotificationCenter
        addNotificationRequest:safe_browsing_notification
         withCompletionHandler:nil];

    base::UmaHistogramEnumeration("IOS.Notifications.SafetyCheck.Requested",
                                  SafetyCheckNotificationType::kSafeBrowsing);

    // In the `kSuccinct` experiment arm, only one notification is allowed at a
    // time. Exit early after scheduling it.
    if (experimental_arm ==
        SafetyCheckNotificationsExperimentalArm::kSuccinct) {
      std::move(completion).Run();
      return;
    }
  }

  UNNotificationRequest* update_chrome_notification =
      UpdateChromeNotificationRequest(update_chrome_state);

  if (update_chrome_notification &&
      AreSafetyCheckUpdateChromeNotificationsAllowed()) {
    [UNUserNotificationCenter.currentNotificationCenter
        addNotificationRequest:update_chrome_notification
         withCompletionHandler:nil];

    base::UmaHistogramEnumeration("IOS.Notifications.SafetyCheck.Requested",
                                  SafetyCheckNotificationType::kUpdateChrome);
  }

  std::move(completion).Run();
}

void SafetyCheckNotificationClient::ClearAndRescheduleSafetyCheckNotifications(
    UpdateChromeSafetyCheckState update_chrome_state,
    SafeBrowsingSafetyCheckState safe_browsing_state,
    PasswordSafetyCheckState password_state,
    password_manager::InsecurePasswordCounts insecure_password_counts,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if ([interacted_notification_metadata_ count]) {
    Browser* browser = GetSceneLevelForegroundActiveBrowser();

    CHECK(browser);

    [HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands)
        prepareToPresentModal:
            base::CallbackToBlock(base::BindOnce(
                &SafetyCheckNotificationClient::ShowUIForNotificationMetadata,
                weak_ptr_factory_.GetWeakPtr(),
                interacted_notification_metadata_, browser))];
  }

  ClearNotifications(
      @[
        kSafetyCheckSafeBrowsingNotificationID,
        kSafetyCheckUpdateChromeNotificationID,
        kSafetyCheckPasswordNotificationID,
      ],
      base::BindOnce(
          &SafetyCheckNotificationClient::ScheduleSafetyCheckNotifications,
          weak_ptr_factory_.GetWeakPtr(), update_chrome_state,
          safe_browsing_state, password_state, insecure_password_counts,
          std::move(completion)));
}

void SafetyCheckNotificationClient::ShowUIForNotificationMetadata(
    NSDictionary* notification_metadata,
    Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The notification metadata must correspond to one of the Safety Check
  // notification types.
  if (!notification_metadata[kSafetyCheckSafeBrowsingNotificationID] &&
      !notification_metadata[kSafetyCheckUpdateChromeNotificationID] &&
      !notification_metadata[kSafetyCheckPasswordNotificationID]) {
    NOTREACHED();
  }

  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);

  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SettingsCommands);

  // If Safe Browsing notification, then show Safe Browsing settings page.
  if (notification_metadata[kSafetyCheckSafeBrowsingNotificationID]) {
    [settingsHandler showSafeBrowsingSettings];

    return;
  }

  // Navigates to the most urgent Safety Check notifications page, prioritizing
  // up-to-date information over potentially stale notification data used
  // during scheduling. This ensures users are directed to the most critical
  // page even if the original notification data is outdated (by at least
  // 'kSafetyCheckNotificationDefaultDelay').
  IOSChromeSafetyCheckManager* safety_check_manager =
      IOSChromeSafetyCheckManagerFactory::GetForProfile(browser->GetProfile());

  // If Update Chrome notification, then show the Chrome App Upgrade page.
  if (notification_metadata[kSafetyCheckUpdateChromeNotificationID]) {
    HandleSafetyCheckUpdateChromeTap(
        safety_check_manager->GetChromeAppUpgradeUrl(), applicationHandler);

    return;
  }

  // If Password notification, then, depending on `insecure_credentials` and
  // `insecure_password_counts`, navigate to the specific page for that insecure
  // credential(s) type.
  if (notification_metadata[kSafetyCheckPasswordNotificationID]) {
    std::vector<password_manager::CredentialUIEntry> insecure_credentials =
        safety_check_manager->GetInsecureCredentials();

    password_manager::InsecurePasswordCounts insecure_password_counts =
        safety_check_manager->GetInsecurePasswordCounts();

    HandleSafetyCheckPasswordTap(insecure_credentials, insecure_password_counts,
                                 applicationHandler, settingsHandler);

    return;
  }
}

void SafetyCheckNotificationClient::LogTriggeredNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();

  const PrefService::Preference* last_sent = local_pref_service->FindPreference(
      prefs::kIosSafetyCheckNotificationsLastSent);

  if (last_sent->IsDefaultValue()) {
    return;
  }

  SafetyCheckNotificationType type =
      static_cast<SafetyCheckNotificationType>(last_sent->GetValue()->GetInt());

  base::UmaHistogramEnumeration("IOS.Notifications.SafetyCheck.Triggered",
                                type);

  local_pref_service->SetInteger(
      prefs::kIosSafetyCheckNotificationsLastTriggered, int(type));

  local_pref_service->ClearPref(prefs::kIosSafetyCheckNotificationsLastSent);
}

void SafetyCheckNotificationClient::LogDismissedNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(task_runner_);

  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();

  if ([interacted_notification_metadata_ count]) {
    local_pref_service->ClearPref(
        prefs::kIosSafetyCheckNotificationsLastTriggered);

    return;
  }

  const PrefService::Preference* last_triggered =
      local_pref_service->FindPreference(
          prefs::kIosSafetyCheckNotificationsLastTriggered);

  if (last_triggered->IsDefaultValue()) {
    return;
  }

  auto completion = base::CallbackToBlock(base::BindPostTask(
      task_runner_,
      base::BindOnce(
          &SafetyCheckNotificationClient::OnGetDeliveredNotifications,
          weak_ptr_factory_.GetWeakPtr())));

  [UNUserNotificationCenter.currentNotificationCenter
      getDeliveredNotificationsWithCompletionHandler:completion];
}

void SafetyCheckNotificationClient::OnGetDeliveredNotifications(
    NSArray<UNNotification*>* notifications) {
  for (UNNotification* notification in notifications) {
    if (ParseSafetyCheckNotificationType(notification.request).has_value()) {
      return;
    }
  }

  // No Safety Check notification was found, so it must have been dismissed.
  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();

  SafetyCheckNotificationType type =
      static_cast<SafetyCheckNotificationType>(local_pref_service->GetInteger(
          prefs::kIosSafetyCheckNotificationsLastTriggered));

  base::UmaHistogramEnumeration("IOS.Notifications.SafetyCheck.Dismissed",
                                type);

  local_pref_service->ClearPref(
      prefs::kIosSafetyCheckNotificationsLastTriggered);
}
