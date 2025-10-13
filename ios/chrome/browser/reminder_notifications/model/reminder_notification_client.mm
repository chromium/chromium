// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_client.h"

#import <optional>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/json/values_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/values.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_mediator.h"
#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_builder.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

ReminderNotificationClient::ReminderNotificationClient(ProfileIOS* profile)
    : PushNotificationClient(PushNotificationClientId::kReminders, profile) {
  CHECK(profile);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();

  PrefService* prefs = profile->GetPrefs();

  pref_change_registrar_->Init(prefs);

  pref_change_registrar_->Add(
      prefs::kReminderNotifications,
      base::BindRepeating(
          &ReminderNotificationClient::OnReminderDataPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_->Add(
      prefs::kFeaturePushNotificationPermissions,
      base::BindRepeating(&ReminderNotificationClient::OnPermissionsPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

ReminderNotificationClient::~ReminderNotificationClient() = default;

std::optional<NotificationType> ReminderNotificationClient::GetNotificationType(
    UNNotification* notification) {
  if (CanHandleNotification(notification)) {
    return NotificationType::kReminder;
  }
  return std::nullopt;
}

bool ReminderNotificationClient::CanHandleNotification(
    UNNotification* notification) {
  return [notification.request.identifier
      hasPrefix:kReminderNotificationsIdentifierPrefix];
}

bool ReminderNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanHandleNotification(response.notification)) {
    return false;
  }

  NSDictionary* user_info = response.notification.request.content.userInfo;
  NSString* url_string = user_info[@"url"];

  if (!url_string || url_string.length == 0) {
    // TODO(crbug.com/422449238): Consider adding UMA logs for missing URL.
    return false;
  }

  GURL url(base::SysNSStringToUTF8(url_string));

  if (!url.is_valid()) {
    // TODO(crbug.com/422449238): Consider adding UMA logs for invalid URL.
    return false;
  }

  // TODO(crbug.com/422449238): Consider adding UMA logs for interaction
  // handling.

  LoadUrlInNewTab(url);

  return true;
}

std::optional<UIBackgroundFetchResult>
ReminderNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* userInfo) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::nullopt;
}

NSArray<UNNotificationCategory*>*
ReminderNotificationClient::RegisterActionableNotifications() {
  return @[];
}

bool ReminderNotificationClient::IsPermitted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ProfileIOS* current_profile = GetProfile();

  return current_profile->GetPrefs()
      ->GetDict(prefs::kFeaturePushNotificationPermissions)
      .FindBool(kReminderNotificationKey)
      .value_or(false);
}

void ReminderNotificationClient::OnReminderDataPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScheduleNewReminders();
}

void ReminderNotificationClient::OnPermissionsPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsPermitted()) {
    CancelAllNotifications(base::DoNothing());

    return;
  }

  ScheduleNewReminders();
}

void ReminderNotificationClient::CancelAllNotifications(
    base::OnceClosure completion_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto completion_block = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          &ReminderNotificationClient::OnGetPendingNotificationsForCancellation,
          weak_ptr_factory_.GetWeakPtr(), std::move(completion_handler))));

  [[UNUserNotificationCenter currentNotificationCenter]
      getPendingNotificationRequestsWithCompletionHandler:completion_block];
}

void ReminderNotificationClient::OnGetPendingNotificationsForCancellation(
    base::OnceClosure completion_handler,
    NSArray<UNNotificationRequest*>* requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NSMutableArray<NSString*>* identifiers_to_remove = [NSMutableArray array];

  for (UNNotificationRequest* request in requests) {
    if ([request.identifier hasPrefix:kReminderNotificationsIdentifierPrefix]) {
      [identifiers_to_remove addObject:request.identifier];
    }
  }

  if (identifiers_to_remove.count > 0) {
    [[UNUserNotificationCenter currentNotificationCenter]
        removePendingNotificationRequestsWithIdentifiers:identifiers_to_remove];
  }

  std::move(completion_handler).Run();
}

void ReminderNotificationClient::ScheduleNotification(
    const GURL& reminder_url,
    const base::Value::Dict& reminder_details,
    std::string_view profile_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!profile_name.empty());
  CHECK(reminder_url.is_valid());

  const base::Value* reminder_time_value =
      reminder_details.Find(kReminderNotificationsTimeKey);

  if (!reminder_time_value) {
    // TODO(crbug.com/422449238): Consider adding UMA/logging for this failure
    // case.
    return;
  }

  std::optional<base::Time> reminder_time =
      base::ValueToTime(reminder_time_value);

  if (!reminder_time.has_value()) {
    // TODO(crbug.com/422449238): Consider adding UMA/logging for this failure
    // case.
    return;
  }

  ReminderNotificationBuilder* builder =
      [[ReminderNotificationBuilder alloc] initWithURL:reminder_url
                                                  time:reminder_time.value()];

  Browser* browser = GetActiveForegroundBrowser();

  web::WebState* web_state =
      browser ? browser->GetWebStateList()->GetActiveWebState() : nullptr;

  if (web_state &&
      (web_state->GetLastCommittedURL() == reminder_url ||
       web_state->GetVisibleURL() == reminder_url) &&
      !web_state->GetTitle().empty()) {
    [builder setPageTitle:base::SysUTF16ToNSString(web_state->GetTitle())];
  }

  // TODO(crbug.com/392921766): Set page image for the notification.

  ScheduledNotificationRequest request = [builder buildRequest];

  ScheduleProfileNotification(
      request,
      base::BindOnce(&ReminderNotificationClient::OnNotificationScheduled,
                     weak_ptr_factory_.GetWeakPtr(), reminder_url),
      profile_name);
}

void ReminderNotificationClient::OnNotificationScheduled(
    const GURL& scheduled_url,
    NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    // TODO(crbug.com/422449238): Consider adding UMA for scheduling failures.
    return;
  }

  ProfileIOS* current_profile = GetProfile();

  if (!current_profile) {
    // Profile might have been destroyed before the callback ran.

    // TODO(crbug.com/422449238): Consider adding UMA for scheduling failures.

    return;
  }

  PrefService* prefs = current_profile->GetPrefs();

  ScopedDictPrefUpdate update(prefs, prefs::kReminderNotifications);

  update->Remove(scheduled_url.spec());

  // TODO(crbug.com/422449238): Consider adding UMA for successful removal.
}

void ReminderNotificationClient::ScheduleNewReminders() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsPermitted()) {
    return;
  }

  auto completion_block = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&ReminderNotificationClient::ScheduleNewRemindersIfNeeded,
                     weak_ptr_factory_.GetWeakPtr())));

  [[UNUserNotificationCenter currentNotificationCenter]
      getPendingNotificationRequestsWithCompletionHandler:completion_block];
}

void ReminderNotificationClient::ScheduleNewRemindersIfNeeded(
    NSArray<UNNotificationRequest*>* pending_requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ProfileIOS* current_profile = GetProfile();
  if (!current_profile || !IsPermitted()) {
    return;
  }

  PrefService* prefs = current_profile->GetPrefs();
  const base::Value::Dict& reminders_in_prefs =
      prefs->GetDict(prefs::kReminderNotifications);
  if (reminders_in_prefs.empty()) {
    return;
  }

  // Build a set of pending URLs for quick lookup.
  std::set<std::string> pending_urls;
  for (UNNotificationRequest* request in pending_requests) {
    if (![request.identifier
            hasPrefix:kReminderNotificationsIdentifierPrefix]) {
      continue;
    }

    NSString* url = request.content.userInfo[@"url"];
    pending_urls.insert(base::SysNSStringToUTF8(url));
  }

  // Iterate through reminders in prefs and schedule notifications only for URLs
  // that exist in prefs but not in the notification center.
  for (const auto [key, value] : reminders_in_prefs) {
    GURL url(key);

    if (!url.is_valid()) {
      continue;
    }

    std::string url_string = url.spec();

    if (pending_urls.find(url_string) == pending_urls.end()) {
      const base::Value::Dict* details = value.GetIfDict();

      if (details) {
        ScheduleNotification(url, *details, current_profile->GetProfileName());
      }
    }
  }
}
