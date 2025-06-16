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
    // TODO(crbug.com/390432325): Consider adding UMA logs for missing URL.
    return false;
  }

  GURL url(base::SysNSStringToUTF8(url_string));

  if (!url.is_valid()) {
    // TODO(crbug.com/390432325): Consider adding UMA logs for invalid URL.
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

  // Schedule notifications based on Pref changes. Cancel existing notifications
  // first, then schedule new ones based on current prefs.
  CancelAllNotifications(base::BindOnce(
      &ReminderNotificationClient::ScheduleNotificationsFromPrefs,
      weak_ptr_factory_.GetWeakPtr()));
}

void ReminderNotificationClient::OnPermissionsPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // When permissions changes, re-evaluate scheduling.
  // Cancel existing notifications first, then attempt to reschedule.
  // `ScheduleNotificationsFromPrefs()` will check the new permission state.
  CancelAllNotifications(base::BindOnce(
      &ReminderNotificationClient::ScheduleNotificationsFromPrefs,
      weak_ptr_factory_.GetWeakPtr()));
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

void ReminderNotificationClient::ScheduleNotificationsFromPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Do not schedule notifications if not permitted.
  if (!IsPermitted()) {
    return;
  }

  ProfileIOS* current_profile = GetProfile();

  PrefService* prefs = current_profile->GetPrefs();

  const base::Value::Dict& reminders =
      prefs->GetDict(prefs::kReminderNotifications);

  if (reminders.empty()) {
    // TODO(crbug.com/422449238): Consider adding UMA/logging for this failure
    // case.
    return;
  }

  for (auto it : reminders) {
    const std::string& url = it.first;
    const base::Value& details = it.second;

    GURL reminder_url(url);

    if (!reminder_url.is_valid()) {
      // TODO(crbug.com/422449238): Consider adding UMA/logging for this failure
      // case.
      continue;
    }

    const base::Value::Dict* reminder_details = details.GetIfDict();

    if (!reminder_details) {
      // TODO(crbug.com/422449238): Consider adding UMA/logging for this failure
      // case.
      continue;
    }

    ScheduleNotification(reminder_url, *reminder_details,
                         current_profile->GetProfileName());
  }
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

  ScheduleProfileNotification(request, base::DoNothing(), profile_name);
}
