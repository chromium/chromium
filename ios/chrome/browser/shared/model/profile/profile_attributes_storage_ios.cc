// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"

#include <stddef.h>

#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"

ProfileAttributesStorageIOS::ProfileAttributesStorageIOS(PrefService* prefs)
    : prefs_(prefs) {
  // Populate the cache.
  for (const auto pair : prefs_->GetDict(prefs::kProfileInfoCache)) {
    sorted_keys_.push_back(pair.first);
  }
  base::ranges::sort(sorted_keys_);

  // If the personal profile name is set, ensure a profile entry with that name
  // actually exists. Note: Can't use `GetPersonalProfileName()` since that
  // DCHECKs that the entry exists.
  const std::string& personal_profile =
      prefs_->GetString(prefs::kPersonalProfileName);
  if (!personal_profile.empty() && !HasProfileWithName(personal_profile)) {
    AddProfile(personal_profile);
  }
}

ProfileAttributesStorageIOS::~ProfileAttributesStorageIOS() = default;

void ProfileAttributesStorageIOS::AddProfile(std::string_view name) {
  // Inserts the profile name in sorted position.
  auto iterator = base::ranges::lower_bound(sorted_keys_, name);
  CHECK(iterator == sorted_keys_.end() || *iterator != name);
  sorted_keys_.insert(iterator, std::string(name));

  // Inserts an empty dictionary for the profile in the preferences.
  {
    ScopedDictPrefUpdate update(prefs_, prefs::kProfileInfoCache);
    ProfileAttributesIOS profile = ProfileAttributesIOS::CreateNew(name);
    update->Set(name, std::move(profile).GetStorage());
  }

  // Update the number of created profile.
  prefs_->SetInteger(prefs::kNumberOfProfiles, sorted_keys_.size());
}

void ProfileAttributesStorageIOS::RemoveProfile(std::string_view name) {
  // The personal profile must always exist, and thus mustn't be deleted.
  DCHECK_NE(name, GetPersonalProfileName());

  // Remove the profile name from the sorted dictionary.
  auto iterator = base::ranges::find(sorted_keys_, name);
  CHECK(iterator != sorted_keys_.end() && *iterator == name);
  sorted_keys_.erase(iterator);

  // Detach any scene that may still be referencing this profile.
  {
    ScopedDictPrefUpdate update(prefs_, prefs::kProfileForScene);

    base::Value::Dict dict;
    for (auto [key, value] : update.Get()) {
      if (value.GetString() != name) {
        dict.Set(key, std::move(value));
      }
    }

    *update = std::move(dict);
  }

  // Update the number of created profile.
  prefs_->SetInteger(prefs::kNumberOfProfiles, sorted_keys_.size());

  // Remove the information about the profile from the preferences.
  {
    ScopedDictPrefUpdate update(prefs_, prefs::kProfileInfoCache);
    update->Remove(name);
  }
}

size_t ProfileAttributesStorageIOS::GetNumberOfProfiles() const {
  return sorted_keys_.size();
}

bool ProfileAttributesStorageIOS::HasProfileWithName(
    std::string_view name) const {
  return GetIndexOfProfileWithName(name) != std::string::npos;
}

ProfileAttributesIOS
ProfileAttributesStorageIOS::GetAttributesForProfileAtIndex(
    size_t index) const {
  CHECK_LT(index, sorted_keys_.size());
  return GetAttributesForProfileWithName(sorted_keys_[index]);
}

ProfileAttributesIOS
ProfileAttributesStorageIOS::GetAttributesForProfileWithName(
    std::string_view name) const {
  const base::Value::Dict& values =
      CHECK_DEREF(prefs_->GetDict(prefs::kProfileInfoCache).FindDict(name));
  return ProfileAttributesIOS::WithAttrs(name, values);
}

void ProfileAttributesStorageIOS::UpdateAttributesForProfileAtIndex(
    size_t index,
    ProfileAttributesCallback callback) {
  CHECK_LT(index, sorted_keys_.size());
  UpdateAttributesForProfileWithName(sorted_keys_[index], std::move(callback));
}

void ProfileAttributesStorageIOS::UpdateAttributesForProfileWithName(
    std::string_view name,
    ProfileAttributesCallback callback) {
  const base::Value::Dict& values =
      CHECK_DEREF(prefs_->GetDict(prefs::kProfileInfoCache).FindDict(name));

  base::Value::Dict updated_values =
      std::move(callback)
          .Run(ProfileAttributesIOS::WithAttrs(name, values))
          .GetStorage();

  if (values != updated_values) {
    ScopedDictPrefUpdate update(prefs_, prefs::kProfileInfoCache);
    update->Set(name, std::move(updated_values));
  }
}

void ProfileAttributesStorageIOS::SetProfileNameForSceneID(
    std::string_view scene_id,
    std::string_view profile_name) {
  DCHECK(!profile_name.empty());
  DCHECK(HasProfileWithName(profile_name));
  ScopedDictPrefUpdate update(prefs_, prefs::kProfileForScene);
  update->Set(scene_id, profile_name);
}

void ProfileAttributesStorageIOS::ClearProfileNameForSceneID(
    std::string_view scene_id) {
  ScopedDictPrefUpdate update(prefs_, prefs::kProfileForScene);
  update->Remove(scene_id);
}

const std::string& ProfileAttributesStorageIOS::GetProfileNameForSceneID(
    std::string_view scene_id) {
  if (const std::string* profile_name =
          prefs_->GetDict(prefs::kProfileForScene).FindString(scene_id)) {
    DCHECK(HasProfileWithName(*profile_name));
    return *profile_name;
  }

  return base::EmptyString();
}

const std::string& ProfileAttributesStorageIOS::GetPersonalProfileName() const {
  const std::string& name = prefs_->GetString(prefs::kPersonalProfileName);
  DCHECK(name.empty() || HasProfileWithName(name));
  return name;
}

void ProfileAttributesStorageIOS::SetPersonalProfileName(
    std::string_view profile_name) {
  DCHECK(!profile_name.empty());
  DCHECK(HasProfileWithName(profile_name));
  prefs_->SetString(prefs::kPersonalProfileName, profile_name);
}

// static
void ProfileAttributesStorageIOS::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProfileInfoCache);
  registry->RegisterIntegerPref(prefs::kNumberOfProfiles, 0);
  registry->RegisterListPref(prefs::kLastActiveProfiles);
  registry->RegisterDictionaryPref(prefs::kProfileForScene);
  registry->RegisterStringPref(prefs::kPersonalProfileName, std::string());
}

size_t ProfileAttributesStorageIOS::GetIndexOfProfileWithName(
    std::string_view name) const {
  auto iterator = base::ranges::lower_bound(sorted_keys_, name);
  if (iterator == sorted_keys_.end() || *iterator != name) {
    return std::string::npos;
  }
  return std::distance(sorted_keys_.begin(), iterator);
}
