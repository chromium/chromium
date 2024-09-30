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
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"

class PrefService;

// Feature used to disable the culling of legacy profiles (i.e. old profile
// dating back from many years ago when a first experimentation was done to
// try to support multi-profiles before WKWebView added the required API in
// iOS 17.0).
BASE_DECLARE_FEATURE(kHideLegacyProfiles);

// ProfileManagerIOS implementation.
class ProfileManagerIOSImpl : public ProfileManagerIOS,
                              public ProfileIOS::Delegate {
 public:
  // Constructs the ProfileManagerIOSImpl with a pointer to the local state's
  // PrefService and with the path to the directory containing the Profiles'
  // data.
  ProfileManagerIOSImpl(PrefService* local_state,
                        const base::FilePath& data_dir);

  ProfileManagerIOSImpl(const ProfileManagerIOSImpl&) = delete;
  ProfileManagerIOSImpl& operator=(const ProfileManagerIOSImpl&) = delete;

  ~ProfileManagerIOSImpl() override;

  // ProfileManagerIOS:
  void AddObserver(ProfileManagerObserverIOS* observer) override;
  void RemoveObserver(ProfileManagerObserverIOS* observer) override;
  void LoadProfiles() override;
  ProfileIOS* GetProfileWithName(std::string_view name) override;
  std::vector<ProfileIOS*> GetLoadedProfiles() override;
  bool LoadProfileAsync(std::string_view name,
                        ProfileLoadedCallback initialized_callback,
                        ProfileLoadedCallback created_callback) override;
  bool CreateProfileAsync(std::string_view name,
                          ProfileLoadedCallback initialized_callback,
                          ProfileLoadedCallback created_callback) override;
  ProfileIOS* LoadProfile(std::string_view name) override;
  ProfileIOS* CreateProfile(std::string_view name) override;
  ProfileAttributesStorageIOS* GetProfileAttributesStorage() override;

  // ProfileIOS::Delegate:
  void OnProfileCreationStarted(
      ProfileIOS* profile,
      ProfileIOS::CreationMode creation_mode) override;
  void OnProfileCreationFinished(ProfileIOS* profile,
                                 ProfileIOS::CreationMode creation_mode,
                                 bool is_new_profile,
                                 bool success) override;

 private:
  class ProfileInfo;

  using CreationMode = ProfileIOS::CreationMode;
  using ProfileMap = std::map<std::string, ProfileInfo, std::less<>>;

  // Returns whether a Profile with `name` exists on disk.
  bool ProfileWithNameExists(std::string_view name);

  // Returns if creating a Profile with `name` is allowed.
  bool CanCreateProfileWithName(std::string_view name);

  // Creates or loads the Profile known by `name` using the `creation_mode`. The
  // callbacks have the same meaning as the method CreateProfileAsync(...).
  // Returns whether a Profile with that name already exists or it can be
  // created.
  bool CreateProfileWithMode(std::string_view name,
                             CreationMode creation_mode,
                             ProfileLoadedCallback initialized_callback,
                             ProfileLoadedCallback created_callback);

  // Final initialization of the profile.
  void DoFinalInit(ProfileIOS* profile);
  void DoFinalInitForServices(ProfileIOS* profile);

  // Hides legacy profiles (i.e. all known profiles not listed in `profiles`).
  void HideLegacyProfiles(const std::set<std::string>& profiles);

  // Restores legacy profiles (if any).
  void RestoreLegacyProfiles(const std::set<std::string>& profiles);

  SEQUENCE_CHECKER(sequence_checker_);

  // The PrefService storing the local state.
  raw_ptr<PrefService> local_state_;

  // The path to the directory where the Profiles' data are stored.
  const base::FilePath profile_data_dir_;

  // Holds the Profile instances that this instance has created.
  ProfileMap profiles_map_;

  // The owned ProfileAttributesStorageIOS instance.
  ProfileAttributesStorageIOS profile_attributes_storage_;

  // The list of registered observers.
  base::ObserverList<ProfileManagerObserverIOS, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_MANAGER_IOS_IMPL_H_
