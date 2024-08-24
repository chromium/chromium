// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"

#include <stddef.h>

#include <utility>

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
  // Populate the cache
  for (const auto pair : prefs_->GetDict(prefs::kProfileInfoCache)) {
    sorted_keys_.push_back(pair.first);
  }
  base::ranges::sort(sorted_keys_);
}

ProfileAttributesStorageIOS::~ProfileAttributesStorageIOS() = default;

void ProfileAttributesStorageIOS::AddProfile(std::string_view name) {
  // Inserts the profile name in sorted position.
  auto iterator = base::ranges::upper_bound(sorted_keys_, name);
  CHECK(iterator == sorted_keys_.end() || *iterator != name);
  sorted_keys_.insert(iterator, std::string(name));

  // Inserts an empty dictionary for the profile in the preferences.
  {
    ScopedDictPrefUpdate update(prefs_, prefs::kProfileInfoCache);
    update->Set(name, base::Value::Dict());
  }

  // Update the number of created profile.
  prefs_->SetInteger(prefs::kBrowserStatesNumCreated, sorted_keys_.size());

  // Insert the newly created profile in the list of last active profiles.
  {
    ScopedListPrefUpdate update(prefs_, prefs::kBrowserStatesLastActive);
    update->Append(base::Value(name));
  }
}

void ProfileAttributesStorageIOS::RemoveProfile(std::string_view name) {
  // Remove the profile name from the sorted dictionary.
  auto iterator = base::ranges::find(sorted_keys_, name);
  CHECK(iterator != sorted_keys_.end() && *iterator == name);
  sorted_keys_.erase(iterator);

  // Remove the profile from the list of last active profiles (if present).
  {
    ScopedListPrefUpdate update(prefs_, prefs::kBrowserStatesLastActive);
    update->EraseValue(base::Value(name));
  }

  // Update the number of created profile.
  prefs_->SetInteger(prefs::kBrowserStatesNumCreated, sorted_keys_.size());

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
  DCHECK_LT(index, sorted_keys_.size());
  const std::string& profile_name = sorted_keys_[index];
  return ProfileAttributesIOS(
      profile_name,
      prefs_->GetDict(prefs::kProfileInfoCache).FindDict(profile_name));
}

ProfileAttributesIOS
ProfileAttributesStorageIOS::GetAttributesForProfileWithName(
    std::string_view name) const {
  const size_t index = GetIndexOfProfileWithName(name);
  return GetAttributesForProfileAtIndex(index);
}

void ProfileAttributesStorageIOS::UpdateAttributesForProfileAtIndex(
    size_t index,
    ProfileAttributesCallback callback) {
  DCHECK_LT(index, sorted_keys_.size());
  const std::string& name = sorted_keys_[index];
  const base::Value::Dict* values =
      prefs_->GetDict(prefs::kProfileInfoCache).FindDict(name);

  base::Value::Dict updated_values =
      std::move(callback).Run(ProfileAttributesIOS(name, values)).GetStorage();
  if (!values || *values != updated_values) {
    ScopedDictPrefUpdate update(prefs_, prefs::kProfileInfoCache);
    update->Set(name, std::move(updated_values));
  }
}

void ProfileAttributesStorageIOS::UpdateAttributesForProfileWithName(
    std::string_view name,
    ProfileAttributesCallback callback) {
  const size_t index = GetIndexOfProfileWithName(name);
  UpdateAttributesForProfileAtIndex(index, std::move(callback));
}

size_t ProfileAttributesStorageIOS::GetIndexOfProfileWithName(
    std::string_view name) const {
  auto iterator = base::ranges::lower_bound(sorted_keys_, name);
  if (iterator == sorted_keys_.end() || *iterator != name) {
    return std::string::npos;
  }
  return std::distance(sorted_keys_.begin(), iterator);
}

void ProfileAttributesStorageIOS::SetBrowserStateForSceneID(
    std::string_view scene_id,
    std::string_view browser_state_name) {
  DCHECK(!browser_state_name.empty());
  DCHECK(HasProfileWithName(browser_state_name));
  ScopedDictPrefUpdate update(prefs_, prefs::kBrowserStateForScene);
  base::Value::Dict& cache = update.Get();
  cache.Set(scene_id, browser_state_name);
}

void ProfileAttributesStorageIOS::ClearBrowserStateForSceneID(
    std::string_view scene_id) {
  ScopedDictPrefUpdate update(prefs_, prefs::kBrowserStateForScene);
  base::Value::Dict& cache = update.Get();
  cache.Remove(scene_id);
}

const std::string& ProfileAttributesStorageIOS::GetBrowserStateNameForSceneID(
    std::string_view scene_id) {
  if (const std::string* browser_state_name =
          prefs_->GetDict(prefs::kBrowserStateForScene).FindString(scene_id)) {
    DCHECK(HasProfileWithName(*browser_state_name));
    return *browser_state_name;
  }

  return base::EmptyString();
}

// static
void ProfileAttributesStorageIOS::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProfileInfoCache);
  registry->RegisterIntegerPref(prefs::kBrowserStatesNumCreated, 0);
  registry->RegisterListPref(prefs::kBrowserStatesLastActive);
  registry->RegisterDictionaryPref(prefs::kBrowserStateForScene);
}
