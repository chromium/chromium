// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/mutable_profile_attributes_storage_ios.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"

MutableProfileAttributesStorageIOS::MutableProfileAttributesStorageIOS(
    PrefService* prefs)
    : ProfileAttributesStorageIOS(prefs) {}

MutableProfileAttributesStorageIOS::~MutableProfileAttributesStorageIOS() =
    default;

void MutableProfileAttributesStorageIOS::AddProfile(
    std::string_view profile_name) {
  // Inserts an empty dictionary for the profile in the preferences.
  {
    ScopedDictPrefUpdate update(prefs_, prefs::kProfileInfoCache);
    ProfileAttributesIOS attr = ProfileAttributesIOS::CreateNew(profile_name);
    update->Set(profile_name, std::move(attr).GetStorage());
  }
}

void MutableProfileAttributesStorageIOS::MarkProfileForDeletion(
    std::string_view profile_name) {
  // The personal profile must always exist, and thus mustn't be deleted.
  DCHECK_NE(profile_name, GetPersonalProfileName());

  // Detach any scene that may still be referencing this profile.
  {
    ScopedDictPrefUpdate update(prefs_, prefs::kProfileForScene);

    base::Value::Dict dict;
    for (auto [key, value] : update.Get()) {
      if (value.GetString() != profile_name) {
        dict.Set(key, std::move(value));
      }
    }

    *update = std::move(dict);
  }

  // Remove the information about the profile from the preferences.
  {
    ScopedDictPrefUpdate update(prefs_, prefs::kProfileInfoCache);
    update->Remove(profile_name);
  }

  // Add the profile to the list of profile marked for deletion.
  {
    ScopedListPrefUpdate update(prefs_, prefs::kProfilesToRemove);
    update->Append(profile_name);
  }
}

void MutableProfileAttributesStorageIOS::ProfileDeletionComplete(
    std::string_view profile_name) {
  // Note: Usually `IsProfileMarkedForDeletion(profile_name)` should be true
  // here, but in some situations the deletion may get triggered twice, and then
  // this method also runs twice, and on the second run the profile will already
  // not be marked for deletion anymore.
  if (!IsProfileMarkedForDeletion(profile_name)) {
    CHECK(base::Contains(deleted_profiles_, profile_name));
    return;
  }
  deleted_profiles_.insert(std::string(profile_name));

  ScopedListPrefUpdate update(prefs_, prefs::kProfilesToRemove);
  update.Get().EraseIf([&profile_name](const base::Value& value) {
    return value == profile_name;
  });
}

bool MutableProfileAttributesStorageIOS::CanDeleteProfileWithName(
    std::string_view name) const {
  // Cannot delete an non-existing profile.
  if (!HasProfileWithName(name)) {
    return false;
  }

  // Cannot delete the personal profile.
  if (name == GetPersonalProfileName()) {
    return false;
  }

  return true;
}

bool MutableProfileAttributesStorageIOS::CanCreateProfileWithName(
    std::string_view name) const {
  // Profile name must not be empty.
  if (name.empty()) {
    return false;
  }

  // Cannot create a profile with the same name as an existing profile.
  if (HasProfileWithName(name)) {
    return false;
  }

  // Cannot create a profile with the same name as a legacy profile.
  if (prefs_->GetDict(prefs::kLegacyProfileMap).Find(name)) {
    return false;
  }

  // Cannot create a profile that have been marked for deletion, and currently
  // not fully deleted yet.
  if (IsProfileMarkedForDeletion(name)) {
    return false;
  }

  return true;
}

std::string MutableProfileAttributesStorageIOS::ReserveNewProfileName() {
  std::string profile_name;
  do {
    const base::Uuid uuid = base::Uuid::GenerateRandomV4();
    profile_name = uuid.AsLowercaseString();
  } while (!CanCreateProfileWithName(profile_name));

  DCHECK(!profile_name.empty());
  DCHECK(!HasProfileWithName(profile_name));
  AddProfile(profile_name);

  return profile_name;
}

void MutableProfileAttributesStorageIOS::EnsurePersonalProfileExists() {
  // Check whether the personal profile preference is set and corresponds to
  // an existing profile. This cannot use GetPersonalProfileName(...) since
  // that method asserts the preference consistency.
  const std::string& personal_profile =
      prefs_->GetString(prefs::kPersonalProfileName);
  if (HasProfileWithName(personal_profile)) {
    return;
  }

  // If the string is set but the profile is marked for deletion then clears
  // the fact that the profile is marked for deletion (by pretending deletion
  // occurred, and then recreating the profile). As the personal profile must
  // not be deleted, this should not happen but prevents a crash if the prefs
  // are inconsistent.
  if (IsProfileMarkedForDeletion(personal_profile)) {
    ProfileDeletionComplete(personal_profile);
    AddProfile(personal_profile);
    return;
  }

  // If there are no known profiles, then this is likely a fresh install and
  // a new profile can be reserved and marked as the personal profile..
  if (!GetNumberOfProfiles()) {
    const std::string new_profile_name = ReserveNewProfileName();
    SetPersonalProfileName(new_profile_name);
    return;
  }

  // There is at least one profile, so use the last used profile (if it is
  // set and valid) as the personal profile.
  const std::string& last_used_profile =
      prefs_->GetString(prefs::kLastUsedProfile);
  if (HasProfileWithName(last_used_profile)) {
    SetPersonalProfileName(last_used_profile);
    return;
  }

  // Clear the last used profile name since it does not exists.
  if (!last_used_profile.empty()) {
    prefs_->ClearPref(prefs::kLastUsedProfile);
  }

  // Pick an arbitrary profile as the personal profile. Either there is
  // a single profile (which corresponds to the case where the user has
  // upgraded from version M-131 or earlier, and it will be marked as
  // the personal profile) or the preferences are in an inconsistent state
  // and an arbitrary profile will be selected.
  IterateOverProfileAttributes(base::BindRepeating(
      [](MutableProfileAttributesStorageIOS& storage,
         const ProfileAttributesIOS& attributes) {
        storage.SetPersonalProfileName(attributes.GetProfileName());
        return ProfileAttributesStorageIOS::IterationResult::kTerminate;
      },
      std::ref(*this)));
}

std::set<std::string>
MutableProfileAttributesStorageIOS::GetProfilesMarkedForDeletion() const {
  std::set<std::string> result;
  std::ranges::transform(
      prefs_->GetList(prefs::kProfilesToRemove),
      std::inserter(result, result.end()),
      [](const base::Value& value) { return value.GetString(); });
  return result;
}
