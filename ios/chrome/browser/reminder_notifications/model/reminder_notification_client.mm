// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_client.h"

#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

ReminderNotificationClient::ReminderNotificationClient(
    ProfileManagerIOS* profile_manager)
    : PushNotificationClient(PushNotificationClientId::kReminders) {
  CHECK(profile_manager);
  profile_manager_observation_.Observe(profile_manager);
}

ReminderNotificationClient::~ReminderNotificationClient() = default;

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

#pragma mark - ProfileManagerObserverIOS

void ReminderNotificationClient::OnProfileManagerDestroyed(
    ProfileManagerIOS* manager) {
  profile_manager_observation_.Reset();
}

void ReminderNotificationClient::OnProfileCreated(ProfileManagerIOS* manager,
                                                  ProfileIOS* profile) {
  // Nothing to do, the Profile is not fully loaded, and it is not possible to
  // access the KeyedService yet.
}

void ReminderNotificationClient::OnProfileLoaded(ProfileManagerIOS* manager,
                                                 ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ObserveProfilePrefs(profile);
}

void ReminderNotificationClient::OnProfileUnloaded(ProfileManagerIOS* manager,
                                                   ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopObservingProfilePrefs(profile);
}

void ReminderNotificationClient::OnProfileMarkedForPermanentDeletion(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {
  // TODO(crbug.com/390687600) Delete notifications when a profile is deleted.
}

void ReminderNotificationClient::ObserveProfilePrefs(ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_observers_.find(profile) != pref_observers_.end()) {
    // Already observing prefs for this profile.
    return;
  }

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar =
      std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar->Init(profile->GetPrefs());
  pref_change_registrar->Add(
      prefs::kReminderNotifications,
      base::BindRepeating(&ReminderNotificationClient::OnProfilePrefsChanged,
                          weak_ptr_factory_.GetWeakPtr(), profile));
  auto [_, inserted] = pref_observers_.insert(
      std::make_pair(profile, std::move(pref_change_registrar)));
  CHECK(inserted);
}

void ReminderNotificationClient::StopObservingProfilePrefs(
    ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iterator = pref_observers_.find(profile);
  CHECK(iterator != pref_observers_.end());
  pref_observers_.erase(iterator);
}

void ReminderNotificationClient::OnProfilePrefsChanged(ProfileIOS* profile) {
  // TODO:(crbug.com/389911697) Schedule notifications based on pref changes.
}
