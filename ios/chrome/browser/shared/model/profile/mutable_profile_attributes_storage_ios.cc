// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/mutable_profile_attributes_storage_ios.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"

MutableProfileAttributesStorageIOS::MutableProfileAttributesStorageIOS(
    PrefService* prefs)
    : ProfileAttributesStorageIOS(prefs) {
  // If the personal profile name is set, ensure a profile entry with that name
  // actually exists. Note: Can't use `GetPersonalProfileName()` since that
  // DCHECKs that the entry exists.
  const std::string& personal_profile =
      prefs_->GetString(prefs::kPersonalProfileName);
  if (!personal_profile.empty() && !HasProfileWithName(personal_profile)) {
    AddProfile(personal_profile);
  }
}

MutableProfileAttributesStorageIOS::~MutableProfileAttributesStorageIOS() =
    default;

void MutableProfileAttributesStorageIOS::AddProfile(std::string_view name) {
  // Inserts the profile name in sorted position.
  auto iterator = std::ranges::lower_bound(sorted_keys_, name);
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

void MutableProfileAttributesStorageIOS::RemoveProfile(std::string_view name) {
  // The personal profile must always exist, and thus mustn't be deleted.
  DCHECK_NE(name, GetPersonalProfileName());

  // Remove the profile name from the sorted dictionary.
  auto iterator = std::ranges::find(sorted_keys_, name);
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
