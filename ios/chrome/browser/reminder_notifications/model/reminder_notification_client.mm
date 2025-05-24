// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_client.h"

#import "base/check.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

ReminderNotificationClient::ReminderNotificationClient(ProfileIOS* profile)
    : PushNotificationClient(PushNotificationClientId::kReminders, profile) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();

  PrefService* prefs = profile->GetPrefs();

  pref_change_registrar_->Init(prefs);

  pref_change_registrar_->Add(
      prefs::kReminderNotifications,
      base::BindRepeating(&ReminderNotificationClient::OnPrefsChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

ReminderNotificationClient::~ReminderNotificationClient() = default;

bool ReminderNotificationClient::CanHandleNotification(
    UNNotification* notification) {
  // TODO(crbug.com/390432325): Handle reminder notification interactions.
  return false;
}

bool ReminderNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/390432325): Handle reminder notification interactions.
  return false;
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

void ReminderNotificationClient::OnSceneActiveForegroundBrowserReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnSceneActiveForegroundBrowserReady(base::DoNothing());
}

void ReminderNotificationClient::OnSceneActiveForegroundBrowserReady(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/390432325): Handle reminder notification interactions.
  std::move(closure).Run();
}

void ReminderNotificationClient::OnPrefsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ProfileIOS* current_profile = GetProfile();

  CHECK(current_profile);

  // TODO:(crbug.com/389911697) Schedule notifications based on pref changes.
}
