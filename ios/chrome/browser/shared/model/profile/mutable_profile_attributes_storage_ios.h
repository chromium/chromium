// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_MUTABLE_PROFILE_ATTRIBUTES_STORAGE_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_MUTABLE_PROFILE_ATTRIBUTES_STORAGE_IOS_H_

#include <set>
#include <string>

#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"

// A sub-class of ProfileAttributesStorageIOS that allow creating or deleting
// profiles (its use is restricted to ProfileManagerIOS implementations).
class MutableProfileAttributesStorageIOS : public ProfileAttributesStorageIOS {
 public:
  explicit MutableProfileAttributesStorageIOS(PrefService* prefs);

  ~MutableProfileAttributesStorageIOS();

  // Register profile with `profile_name`. No profile with that name must be
  // registered yet.
  void AddProfile(std::string_view profile_name);

  // Mark `profile_name` for deletion and remove any stored attributes. This
  // removes all attributes for the profile, it won't show up when iterating
  // over all profiles, it is no longer counted in GetNumberOfProfiles() but
  // accessing the attributes returns a special "deleted profile" object and
  // updates by name are ignored (but do not crash).
  void MarkProfileForDeletion(std::string_view profile_name);

  // Mark `profile_name` as fully deleted from disk.
  void ProfileDeletionComplete(std::string_view profile_name);

  // Returns whether a profile with `name` can be deleted.
  bool CanDeleteProfileWithName(std::string_view name) const;

  // Returns whether a profile with `name` can be created.
  bool CanCreateProfileWithName(std::string_view name) const;

  // Reserves a new randomly generated name that can be used to create a new
  // profile and returns the new name. The profile will be registered and its
  // attributes can be set immediately.
  std::string ReserveNewProfileName();

  // Ensures that the "personal profile" exists.
  void EnsurePersonalProfileExists();

  // Returns the list of all profiles marked for deletion.
  std::set<std::string> GetProfilesMarkedForDeletion() const;

 private:
  // Names of all deleted profiles in the current app execution.
  std::set<std::string, std::less<>> deleted_profiles_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_MUTABLE_PROFILE_ATTRIBUTES_STORAGE_IOS_H_
