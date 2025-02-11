// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_observer_ios.h"

ProfileAttributesStorageIOS::ProfileAttributesStorageIOS(PrefService* prefs)
    : prefs_(prefs) {
  // Populate the cache.
  for (const auto pair : prefs_->GetDict(prefs::kProfileInfoCache)) {
    sorted_keys_.push_back(pair.first);
  }
  std::ranges::sort(sorted_keys_);
}

ProfileAttributesStorageIOS::~ProfileAttributesStorageIOS() = default;

void ProfileAttributesStorageIOS::AddObserver(
    ProfileAttributesStorageObserverIOS* observer) {
  observers_.AddObserver(observer);
}

void ProfileAttributesStorageIOS::RemoveObserver(
    ProfileAttributesStorageObserverIOS* observer) {
  observers_.RemoveObserver(observer);
}

size_t ProfileAttributesStorageIOS::GetNumberOfProfiles() const {
  return sorted_keys_.size();
}

bool ProfileAttributesStorageIOS::HasProfileWithName(
    std::string_view name) const {
  return GetIndexOfProfileWithName(name) != std::string::npos;
}

bool ProfileAttributesStorageIOS::IsProfileMarkedForDeletion(
    std::string_view profile_name) const {
  return base::Contains(prefs_->GetList(prefs::kProfilesToRemove),
                        profile_name);
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
  if (IsProfileMarkedForDeletion(name)) {
    return ProfileAttributesIOS::DeletedProfile(name);
  }

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
  if (IsProfileMarkedForDeletion(name)) {
    return;
  }

  const base::Value::Dict& values =
      CHECK_DEREF(prefs_->GetDict(prefs::kProfileInfoCache).FindDict(name));

  ProfileAttributesIOS attr =
      std::move(callback).Run(ProfileAttributesIOS::WithAttrs(name, values));
  CHECK(!attr.IsDeletedProfile());

  base::Value::Dict updated_values = std::move(attr).GetStorage();
  if (values != updated_values) {
    // Note: The block is there to ensure the pref update gets committed before
    // observers are notified, so they see the new value.
    {
      ScopedDictPrefUpdate update(prefs_, prefs::kProfileInfoCache);
      update->Set(name, std::move(updated_values));
    }
    observers_.Notify(
        &ProfileAttributesStorageObserverIOS::OnProfileAttributesUpdated, name);
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
  registry->RegisterListPref(prefs::kProfilesToRemove);
}

size_t ProfileAttributesStorageIOS::GetIndexOfProfileWithName(
    std::string_view name) const {
  auto iterator = std::ranges::lower_bound(sorted_keys_, name);
  if (iterator == sorted_keys_.end() || *iterator != name) {
    return std::string::npos;
  }
  return std::distance(sorted_keys_.begin(), iterator);
}
