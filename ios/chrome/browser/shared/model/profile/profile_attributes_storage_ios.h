// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_STORAGE_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_STORAGE_IOS_H_

#include <stddef.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"

class PrefRegistrySimple;
class PrefService;
class ProfileAttributesIOS;

// This class saves various information about profiles to local preferences.
class ProfileAttributesStorageIOS {
 public:
  // Callback that can modify and then return a ProfileAttributesIOS.
  using ProfileAttributesCallback =
      base::OnceCallback<ProfileAttributesIOS(ProfileAttributesIOS)>;

  explicit ProfileAttributesStorageIOS(PrefService* prefs);

  ProfileAttributesStorageIOS(const ProfileAttributesStorageIOS&) = delete;
  ProfileAttributesStorageIOS& operator=(const ProfileAttributesStorageIOS&) =
      delete;

  ~ProfileAttributesStorageIOS();

  // Register profile with `name`.
  void AddProfile(std::string_view name);

  // Remove informations about profile with `name`.
  void RemoveProfile(std::string_view name);

  // Returns the count of known profiles.
  size_t GetNumberOfProfiles() const;

  // Returns whether a profile with `name` exists.
  bool HasProfileWithName(std::string_view name) const;

  // Retrieves the information for profile at `index`. Note that the ordering of
  // profiles is arbitrary, so this is mostly useful for "for each profile"
  // types of usage.
  ProfileAttributesIOS GetAttributesForProfileAtIndex(size_t index) const;

  // Retrieves the information for profile with `name`.
  ProfileAttributesIOS GetAttributesForProfileWithName(
      std::string_view name) const;

  // Modifies the attributes for the profile with `name` (which must exist).
  // The callback is invoked synchronously and can modify the attributes
  // and the data will be saved to the preferences if changed.
  void UpdateAttributesForProfileWithName(std::string_view name,
                                          ProfileAttributesCallback callback);

  // Register the given profile with the given scene.
  void SetProfileNameForSceneID(std::string_view scene_id,
                                std::string_view profile_name);

  // Removes the given scene records.
  void ClearProfileNameForSceneID(std::string_view scene_id);

  // Returns the name of the profile associated to the given scene.
  const std::string& GetProfileNameForSceneID(std::string_view scene_id);

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Modifies the attributes for the profile at `index` (which must exist).
  // The callback is invoked synchronously and can modify the attributes
  // and the data will be saved to the preferences if changed.
  void UpdateAttributesForProfileAtIndex(size_t index,
                                         ProfileAttributesCallback callback);

  // Returns the index of the profile with `name` or std::string::npos if
  // not found.
  size_t GetIndexOfProfileWithName(std::string_view name) const;

  raw_ptr<PrefService> prefs_;
  // All known profile names, sorted alphabetically.
  std::vector<std::string> sorted_keys_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_STORAGE_IOS_H_
