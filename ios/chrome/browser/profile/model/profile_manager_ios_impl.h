// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_MANAGER_IOS_IMPL_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_MANAGER_IOS_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/profile/model/profile_deleter_ios.h"
#include "ios/chrome/browser/shared/model/profile/mutable_profile_attributes_storage_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"

class PrefService;

// ProfileManagerIOS implementation.
class ProfileManagerIOSImpl : public ProfileManagerIOS,
                              public ProfileIOS::Delegate {
 public:
  // For ProfileIOS::Delegate methods.
  using CreationMode = ProfileIOS::CreationMode;

  // Constructs the ProfileManagerIOSImpl with a pointer to the local state's
  // PrefService and with the path to the directory containing the Profiles'
  // data.
  ProfileManagerIOSImpl(PrefService* local_state,
                        const base::FilePath& data_dir);

  ProfileManagerIOSImpl(const ProfileManagerIOSImpl&) = delete;
  ProfileManagerIOSImpl& operator=(const ProfileManagerIOSImpl&) = delete;

  ~ProfileManagerIOSImpl() override;

  // ProfileManagerIOS:
  void PrepareForDestruction() override;
  void AddObserver(ProfileManagerObserverIOS* observer) override;
  void RemoveObserver(ProfileManagerObserverIOS* observer) override;
  ProfileIOS* GetProfileWithName(std::string_view name) override;
  std::vector<ProfileIOS*> GetLoadedProfiles() const override;
  bool HasProfileWithName(std::string_view name) const override;
  bool CanCreateProfileWithName(std::string_view name) const override;
  std::string ReserveNewProfileName() override;
  bool CanDeleteProfileWithName(std::string_view name) const override;
  bool LoadProfileAsync(std::string_view name,
                        ProfileLoadedCallback initialized_callback,
                        ProfileLoadedCallback created_callback) override;
  bool CreateProfileAsync(std::string_view name,
                          ProfileLoadedCallback initialized_callback,
                          ProfileLoadedCallback created_callback) override;
  void MarkProfileForDeletion(std::string_view name) override;
  bool IsProfileMarkedForDeletion(std::string_view name) const override;
  void PurgeProfilesMarkedForDeletion(base::OnceClosure callback) override;
  ProfileAttributesStorageIOS* GetProfileAttributesStorage() override;

  // ProfileIOS::Delegate:
  void OnProfileCreationStarted(ProfileIOS* profile,
                                CreationMode creation_mode) override;
  void OnProfileCreationFinished(ProfileIOS* profile,
                                 CreationMode creation_mode,
                                 bool is_new_profile,
                                 bool success) override;

 private:
  class ProfileInfo;

  // Represents how CreateOrLoadProfile(...) should behave if the profile
  // does not exists on disk yet.
  enum class LoadOrCreatePolicy {
    kLoadOnly,
    kCreateIfNecessary,
  };

  // Creates or loads the profile known by `name`. Helper function used to
  // implement both `CreateProfileAsync()` and `LoadProfileAsync()`. The
  // callback parameters have the same meaning as those two methods. If
  // the profile does not exists on disk and `policy` is `kLoadOnly` then
  // the method will fail and return false, otherwise it will create the
  // profile if necessary and always succeed.
  bool CreateOrLoadProfile(std::string_view name,
                           LoadOrCreatePolicy policy,
                           ProfileLoadedCallback initialized_callback,
                           ProfileLoadedCallback created_callback);

  // Final initialization of the profile.
  void DoFinalInit(ProfileIOS* profile);
  void DoFinalInitForServices(ProfileIOS* profile);

  // Invoked when a profile deletion attempts complete with success or not.
  // Will invoke `closure` after updating the ProfileAttributesStorageIOS.
  void OnProfileDeletionComplete(base::OnceClosure closure,
                                 const std::string& profile_name,
                                 ProfileDeleterIOS::Result result);

  // Returns a ScopedProfileKeepAliveIOS for `info` (which may be null if
  // the profile could not be loaded).
  ScopedProfileKeepAliveIOS CreateScopedProfileKeepAlive(ProfileInfo* info);

  // Called when a ScopedProfileKeepAliveIOS is destroyed. Will unload the
  // profile if no other ScopedProfileKeepAliveIOS reference it.
  void MaybeUnloadProfile(std::string_view name);

  SEQUENCE_CHECKER(sequence_checker_);

  // The PrefService storing the local state.
  raw_ptr<PrefService> local_state_;

  // The path to the directory where the Profiles' data are stored.
  const base::FilePath profile_data_dir_;

  // Holds the Profile instances that this instance has created.
  std::map<std::string, ProfileInfo, std::less<>> profiles_map_;

  // Holds the ScopedProfileKeepAliveIOS for the profiles that are loading.
  std::map<std::string, ScopedProfileKeepAliveIOS, std::less<>>
      loading_profiles_map_;

  // The owned ProfileAttributesStorageIOS instance.
  MutableProfileAttributesStorageIOS profile_attributes_storage_;

  // The owned ProfileDeleterIOS instance.
  ProfileDeleterIOS profile_deleter_;

  // The list of registered observers.
  base::ObserverList<ProfileManagerObserverIOS, true> observers_;

  // Record whether the manager will soon be destroyed and loading
  // profile is now forbidden.
  bool will_be_destroyed_ = false;

  // Factory for weak pointers.
  base::WeakPtrFactory<ProfileManagerIOSImpl> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_MANAGER_IOS_IMPL_H_
