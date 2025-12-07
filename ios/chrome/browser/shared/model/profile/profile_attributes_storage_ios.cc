// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"

#include <stddef.h>

#include <algorithm>
#include <type_traits>
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

namespace {

using IterationResult = ProfileAttributesStorageIOS::IterationResult;

// Adaptor that always return kContinue.
ProfileAttributesStorageIOS::IterationResult ReturnContinue() {
  return ProfileAttributesStorageIOS::IterationResult::kContinue;
}

// Helper to implement iteration over all attributes.
template <typename AttributesReference, typename MaybeConstDict>
void IterateOverProfileAttributesImpl(
    MaybeConstDict& dict,
    base::RepeatingCallback<IterationResult(AttributesReference)> iterator) {
  for (auto pair : dict) {
    MaybeConstDict& value = pair.second.GetDict();
    ProfileAttributesIOS attr =
        ProfileAttributesIOS::WithAttrs(pair.first, value);

    // Call the iterator, but do not check the value immediately, first,
    // update the dictionary if attr has been modified.
    const IterationResult result = iterator.Run(attr);

    // Only check for mutation if the iterator parameter is a non-const
    // reference (it is not possible for the iterator to mutate to value
    // otherwise).
    if constexpr (std::is_same_v<AttributesReference, ProfileAttributesIOS&>) {
      base::Value::Dict storage = std::move(attr).GetStorage();
      if (storage != value) {
        value = std::move(storage);
      }
    }

    if (result == IterationResult::kTerminate) {
      break;
    }
  }
}

}  // anonymous namespace

ProfileAttributesStorageIOS::ProfileAttributesStorageIOS(PrefService* prefs)
    : prefs_(CHECK_DEREF(prefs)) {
  // Some users are crashing on startup because kProfileInfoCache values are
  // not dictionaries (see https://crbug.com/426651506 for details). Iterate
  // over the preference and remove all invalid values.
  std::set<std::string> keys_to_remove;
  for (const auto pair : prefs_->GetDict(prefs::kProfileInfoCache)) {
    if (!pair.second.is_dict()) {
      keys_to_remove.insert(pair.first);
    }
  }

  // If there are any keys whose value is not a dictionary, then ...
  if (!keys_to_remove.empty()) {
    // ... first remove them from `kProfileInfoCache`, ...
    {
      ScopedDictPrefUpdate update(&prefs_.get(), prefs::kProfileInfoCache);
      base::Value::Dict& dict = update.Get();
      for (const auto& key : keys_to_remove) {
        dict.Remove(key);
      }
    }

    // ... second request the removal of the corresponding profiles,
    // unless it was already requested.
    {
      ScopedListPrefUpdate update(&prefs_.get(), prefs::kProfilesToRemove);
      base::Value::List& list = update.Get();
      for (const auto& value : list) {
        const std::string& key = value.GetString();
        if (base::Contains(keys_to_remove, key)) {
          keys_to_remove.erase(key);
        }
      }

      for (const auto& key : keys_to_remove) {
        list.Append(key);
      }
    }
  }
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
  return prefs_->GetDict(prefs::kProfileInfoCache).size();
}

bool ProfileAttributesStorageIOS::HasProfileWithName(
    std::string_view name) const {
  if (name.empty()) {
    return false;
  }

  return prefs_->GetDict(prefs::kProfileInfoCache).FindDict(name) != nullptr;
}

bool ProfileAttributesStorageIOS::IsProfileMarkedForDeletion(
    std::string_view profile_name) const {
  return base::Contains(prefs_->GetList(prefs::kProfilesToRemove),
                        profile_name);
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

void ProfileAttributesStorageIOS::UpdateAttributesForProfileWithName(
    std::string_view name,
    ProfileAttributesCallback callback) {
  if (IsProfileMarkedForDeletion(name)) {
    return;
  }

  const base::Value::Dict& values =
      CHECK_DEREF(prefs_->GetDict(prefs::kProfileInfoCache).FindDict(name));

  ProfileAttributesIOS attr = ProfileAttributesIOS::WithAttrs(name, values);
  std::move(callback).Run(attr);
  CHECK(!attr.IsDeletedProfile());

  base::Value::Dict updated_values = std::move(attr).GetStorage();
  if (values != updated_values) {
    // Note: The block is there to ensure the pref update gets committed before
    // observers are notified, so they see the new value.
    {
      ScopedDictPrefUpdate update(&prefs_.get(), prefs::kProfileInfoCache);
      update->Set(name, std::move(updated_values));
    }
    observers_.Notify(
        &ProfileAttributesStorageObserverIOS::OnProfileAttributesUpdated, name);
  }
}

void ProfileAttributesStorageIOS::IterateOverProfileAttributes(
    Iterator iterator) {
  ScopedDictPrefUpdate update(&prefs_.get(), prefs::kProfileInfoCache);
  IterateOverProfileAttributesImpl(update.Get(), std::move(iterator));
}

void ProfileAttributesStorageIOS::IterateOverProfileAttributes(
    CompleteIterator iterator) {
  IterateOverProfileAttributes(
      std::move(iterator).Then(base::BindRepeating(&ReturnContinue)));
}

void ProfileAttributesStorageIOS::IterateOverProfileAttributes(
    ConstIterator iterator) const {
  IterateOverProfileAttributesImpl(prefs_->GetDict(prefs::kProfileInfoCache),
                                   std::move(iterator));
}

void ProfileAttributesStorageIOS::IterateOverProfileAttributes(
    ConstCompleteIterator iterator) const {
  IterateOverProfileAttributes(
      std::move(iterator).Then(base::BindRepeating(&ReturnContinue)));
}

void ProfileAttributesStorageIOS::SetProfileNameForSceneID(
    std::string_view scene_id,
    std::string_view profile_name) {
  DCHECK(!profile_name.empty());
  DCHECK(HasProfileWithName(profile_name));
  ScopedDictPrefUpdate update(&prefs_.get(), prefs::kProfileForScene);
  update->Set(scene_id, profile_name);
}

void ProfileAttributesStorageIOS::ClearProfileNameForSceneID(
    std::string_view scene_id) {
  ScopedDictPrefUpdate update(&prefs_.get(), prefs::kProfileForScene);
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
  registry->RegisterDictionaryPref(prefs::kProfileForScene);
  registry->RegisterStringPref(prefs::kPersonalProfileName, std::string());
  registry->RegisterListPref(prefs::kProfilesToRemove);
}
