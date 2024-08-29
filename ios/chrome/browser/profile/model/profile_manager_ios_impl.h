// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_MANAGER_IOS_IMPL_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_MANAGER_IOS_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"

class PrefService;

// ProfileManagerIOS implementation.
class ProfileManagerIOSImpl : public ProfileManagerIOS,
                              public ChromeBrowserState::Delegate {
 public:
  // Constructs the ProfileManagerIOSImpl with a pointer to the local
  // state's PrefService and with the path to the directory containing the
  // ChromeBrowserStates' data.
  ProfileManagerIOSImpl(PrefService* local_state,
                        const base::FilePath& data_dir);

  ProfileManagerIOSImpl(const ProfileManagerIOSImpl&) = delete;
  ProfileManagerIOSImpl& operator=(const ProfileManagerIOSImpl&) = delete;

  ~ProfileManagerIOSImpl() override;

  // ProfileManagerIOS:
  void AddObserver(ProfileManagerObserverIOS* observer) override;
  void RemoveObserver(ProfileManagerObserverIOS* observer) override;
  void LoadBrowserStates() override;
  ChromeBrowserState* GetLastUsedBrowserStateDeprecatedDoNotUse() override;
  ChromeBrowserState* GetProfileWithName(std::string_view name) override;
  std::vector<ChromeBrowserState*> GetLoadedProfiles() override;
  bool LoadBrowserStateAsync(std::string_view name,
                             ProfileLoadedCallback initialized_callback,
                             ProfileLoadedCallback created_callback) override;
  bool CreateBrowserStateAsync(std::string_view name,
                               ProfileLoadedCallback initialized_callback,
                               ProfileLoadedCallback created_callback) override;
  ChromeBrowserState* LoadBrowserState(std::string_view name) override;
  ChromeBrowserState* CreateBrowserState(std::string_view name) override;
  ProfileAttributesStorageIOS* GetProfileAttributesStorage() override;

  // ChromeBrowserState::Delegate:
  void OnChromeBrowserStateCreationStarted(
      ChromeBrowserState* browser_state,
      ChromeBrowserState::CreationMode creation_mode) override;
  void OnChromeBrowserStateCreationFinished(
      ChromeBrowserState* browser_state,
      ChromeBrowserState::CreationMode creation_mode,
      bool is_new_browser_state,
      bool success) override;

 private:
  class BrowserStateInfo;

  using CreationMode = ChromeBrowserState::CreationMode;
  using ChromeBrowserMap = std::map<std::string, BrowserStateInfo, std::less<>>;

  // Get the name of the last used browser state, or if that's undefined, the
  // default browser state.
  std::string GetLastUsedBrowserStateName() const;

  // Returns whether a Profile with `name` exists on disk.
  bool ProfileWithNameExists(std::string_view name);

  // Returns if creating a ChromeBrowserState with `name` is allowed.
  bool CanCreateBrowserStateWithName(std::string_view name);

  // Creates or loads the ChromeBrowserState known by `name` using the
  // `creation_mode`. The callbacks have the same meaning as the method
  // CreateBrowserStateAsync(...). Returns whether a ChromeBrowserState
  // with that name already exists or it can be created.
  bool CreateBrowserStateWithMode(std::string_view name,
                                  CreationMode creation_mode,
                                  ProfileLoadedCallback initialized_callback,
                                  ProfileLoadedCallback created_callback);

  // Final initialization of the browser state.
  void DoFinalInit(ChromeBrowserState* browser_state);
  void DoFinalInitForServices(ChromeBrowserState* browser_state);

  SEQUENCE_CHECKER(sequence_checker_);

  // The PrefService storing the local state.
  raw_ptr<PrefService> local_state_;

  // The path to the directory where the ChromeBrowserStates are stored.
  const base::FilePath data_dir_;

  // Holds the ChromeBrowserState instances that this instance has created.
  ChromeBrowserMap browser_states_;

  // The owned ProfileAttributesStorageIOS instance.
  ProfileAttributesStorageIOS profile_attributes_storage_;

  // The list of registered observers.
  base::ObserverList<ProfileManagerObserverIOS, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_MANAGER_IOS_IMPL_H_
