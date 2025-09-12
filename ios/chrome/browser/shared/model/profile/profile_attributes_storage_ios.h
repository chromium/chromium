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
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/values.h"

class PrefRegistrySimple;
class PrefService;
class ProfileAttributesIOS;
class ProfileAttributesStorageObserverIOS;

// This class saves various information about profiles to local preferences.
class ProfileAttributesStorageIOS {
 public:
  // Enum controlling the iteration behaviour.
  enum class IterationResult {
    kContinue,
    kTerminate,
  };

  // Callback that can modify a ProfileAttributesIOS.
  using ProfileAttributesCallback =
      base::OnceCallback<void(ProfileAttributesIOS&)>;

  // Iterator types for the IterateOverProfileAttributes(...) overloads.
  //
  // The iterators returning a IterationResult may stop the iteration by
  // returning IterationResult::kTerminate, the iterators returning void
  // will always iterate over all items.
  //
  // The iterator taking the ProfileAttributesIOS by reference can mutate
  // the object and the changes (if any) will be reflected in the storage.
  using Iterator =
      base::RepeatingCallback<IterationResult(ProfileAttributesIOS&)>;
  using CompleteIterator = base::RepeatingCallback<void(ProfileAttributesIOS&)>;
  using ConstIterator =
      base::RepeatingCallback<IterationResult(const ProfileAttributesIOS&)>;
  using ConstCompleteIterator =
      base::RepeatingCallback<void(const ProfileAttributesIOS&)>;

  explicit ProfileAttributesStorageIOS(PrefService* prefs);

  ProfileAttributesStorageIOS(const ProfileAttributesStorageIOS&) = delete;
  ProfileAttributesStorageIOS& operator=(const ProfileAttributesStorageIOS&) =
      delete;

  ~ProfileAttributesStorageIOS();

  // Register/unregister `observer`.
  void AddObserver(ProfileAttributesStorageObserverIOS* observer);
  void RemoveObserver(ProfileAttributesStorageObserverIOS* observer);

  // Returns the count of known profiles.
  size_t GetNumberOfProfiles() const;

  // Returns whether a profile with `name` exists.
  bool HasProfileWithName(std::string_view name) const;

  // Returns whether the profile is marked for deletion.
  bool IsProfileMarkedForDeletion(std::string_view profile_name) const;

  // Retrieves the information for profile with `name`.
  ProfileAttributesIOS GetAttributesForProfileWithName(
      std::string_view name) const;

  // Modifies the attributes for the profile with `name` (which must exist).
  // The callback is invoked synchronously and can modify the attributes
  // and the data will be saved to the preferences if changed.
  void UpdateAttributesForProfileWithName(std::string_view name,
                                          ProfileAttributesCallback callback);

  // Iterates over profiles' attributes, stopping the iteration if `iterator`
  // return IterationResult::kTerminate. Can mutate the attributes and the
  // changes will be reflected in the storage.
  void IterateOverProfileAttributes(Iterator iterator);

  // Adaptor for IterateOverProfileAttributes(...) which accept an iterator
  // that returns void and which iterate over all values.  Can mutate the
  // attributes and the changes will be reflected in the storage.
  void IterateOverProfileAttributes(CompleteIterator iterator);

  // Iterates over profiles' attributes, stopping the iteration if `iterator`
  // return IterationResult::kTerminate. Can mutate the attributes and the
  // changes will be reflected in the storage.
  void IterateOverProfileAttributes(ConstIterator iterator) const;

  // Adaptor for IterateOverProfileAttributes(...) which accept an iterator
  // that returns void and which iterate over all values.  Can mutate the
  // attributes and the changes will be reflected in the storage.
  void IterateOverProfileAttributes(ConstCompleteIterator iterator) const;

  // Register the given profile with the given scene.
  void SetProfileNameForSceneID(std::string_view scene_id,
                                std::string_view profile_name);

  // Removes the given scene records.
  void ClearProfileNameForSceneID(std::string_view scene_id);

  // Returns the name of the profile associated to the given scene.
  const std::string& GetProfileNameForSceneID(std::string_view scene_id);

  // Returns the name of the profile that's designated as the personal profile,
  // i.e. the one containing all consumer (non-managed) accounts. The name may
  // be empty (should only happen very briefly during startup), but otherwise
  // it's guaranteed that an entry with this name exists.
  const std::string& GetPersonalProfileName() const;

  // Designates the profile with `profile_name` as the personal profile. A
  // profile entry with this name must already exist.
  void SetPersonalProfileName(std::string_view profile_name);

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  const raw_ref<PrefService> prefs_;

  base::ObserverList<ProfileAttributesStorageObserverIOS, /*check_empty=*/true>
      observers_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_STORAGE_IOS_H_
