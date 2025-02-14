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
  CHECK(IsProfileMarkedForDeletion(profile_name));
  ScopedListPrefUpdate update(prefs_, prefs::kProfilesToRemove);
  update.Get().EraseIf([&profile_name](const base::Value& value) {
    return value == profile_name;
  });
}
