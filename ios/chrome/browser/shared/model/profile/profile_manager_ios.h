// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_IOS_H_

#import <string>
#import <string_view>
#import <vector>

#import "base/functional/callback.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ProfileAttributesStorageIOS;
class ProfileManagerObserverIOS;

// Provides methods that allow for various ways of creating non-incognito
// Profile instances. Owns all instances that it creates.
class ProfileManagerIOS {
 public:
  // Callback invoked when a Profile has been loaded asynchronously.
  using ProfileLoadedCallback = base::OnceCallback<void(ProfileIOS*)>;

  ProfileManagerIOS(const ProfileManagerIOS&) = delete;
  ProfileManagerIOS& operator=(const ProfileManagerIOS&) = delete;

  virtual ~ProfileManagerIOS() {}

  // Registers/unregisters observers.
  virtual void AddObserver(ProfileManagerObserverIOS* observer) = 0;
  virtual void RemoveObserver(ProfileManagerObserverIOS* observer) = 0;

  // Loads the last active profiles. *Deprecated*.
  virtual void LoadProfiles() = 0;

  // Returns the Profile known by `name` or nullptr if there is no loaded
  // Profiles with that `name`.
  virtual ProfileIOS* GetProfileWithName(std::string_view name) = 0;

  // Returns the list of loaded Profiles. The order is arbitrary.
  virtual std::vector<ProfileIOS*> GetLoadedProfiles() = 0;

  // Asynchronously loads a Profile known by `name` if it exists. The
  // `created_callback` will be called with the Profile when it has been created
  // (but not yet initialised) and `initialised_callback` will be called once
  // the Profile is fully initialised. Returns true if the Profile exists, false
  // otherwise.
  //
  // In case of failure, `initialized_callback` is invoked with nullptr. The
  // `created_callback` will only be called if the Profile is created, and thus
  // will never receive nullptr but may never be called if the creation is
  // disallowed.
  virtual bool LoadProfileAsync(
      std::string_view name,
      ProfileLoadedCallback initialized_callback,
      ProfileLoadedCallback created_callback = {}) = 0;

  // Asynchronously creates or loads a Profile known by `name`. The callback
  // `create_callback` will be called with the Profile when it has been created
  // (but not yet initialised) and `initialised_callback` will be called once
  // the Profile is fully initialised. Returns true if the Profile exists or can
  // be created, false otherwise.
  //
  // In case of failure, `initialized_callback` is invoked with nullptr. The
  // `created_callback` will only be called if the Profile is created, and thus
  // will never receive nullptr but may never be called if the creation is
  // disallowed.
  virtual bool CreateProfileAsync(
      std::string_view name,
      ProfileLoadedCallback initialized_callback,
      ProfileLoadedCallback created_callback = {}) = 0;

  // Loads the Profile known by `name` and returns it. As this method is
  // synchronous, it may block the application so it should only be used during
  // the initialisation when blocking is possible or for tests. Returns null if
  // loading the Profile failed.
  virtual ProfileIOS* LoadProfile(std::string_view name) = 0;

  // Creates or loads the Profile known by `name` and returns it. As this method
  // is synchronous, it may block the application so it should only be used
  // during the initialisation when blocking is possible or for tests. Returns
  // null if loading or creating the Profile failed.
  virtual ProfileIOS* CreateProfile(std::string_view name) = 0;

  // Returns the ProfileAttributesStorageIOS associated with this manager.
  virtual ProfileAttributesStorageIOS* GetProfileAttributesStorage() = 0;

 protected:
  ProfileManagerIOS() {}
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_IOS_H_
