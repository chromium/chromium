// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_IOS_H_

#import <string>
#import <string_view>
#import <vector>

#import "base/functional/callback.h"
#import "base/types/pass_key.h"

class ProfileAttributesStorageIOS;
class ProfileIOS;
class ProfileManagerObserverIOS;
class ScopedProfileKeepAliveIOS;

// Provides methods that allow for various ways of creating non-incognito
// Profile instances. Owns all instances that it creates.
class ProfileManagerIOS {
 public:
  // PassKey to create ScopedProfileKeepAliveIOS.
  using PassKey = base::PassKey<ProfileManagerIOS>;

  // Callback invoked when a Profile has been loaded asynchronously.
  using ProfileLoadedCallback =
      base::OnceCallback<void(ScopedProfileKeepAliveIOS)>;

  ProfileManagerIOS(const ProfileManagerIOS&) = delete;
  ProfileManagerIOS& operator=(const ProfileManagerIOS&) = delete;

  virtual ~ProfileManagerIOS() {}

  // Informs the ProfileManager that it will be destroyed and should ensure
  // that all profiles are unloaded.
  virtual void PrepareForDestruction() = 0;

  // Registers/unregisters observers.
  virtual void AddObserver(ProfileManagerObserverIOS* observer) = 0;
  virtual void RemoveObserver(ProfileManagerObserverIOS* observer) = 0;

  // Returns the Profile known by `name` or nullptr if there is no loaded
  // Profiles with that `name`.
  virtual ProfileIOS* GetProfileWithName(std::string_view name) = 0;

  // Returns the list of loaded Profiles. The order is arbitrary.
  virtual std::vector<ProfileIOS*> GetLoadedProfiles() const = 0;

  // Returns whether a profile with `name` exists (it may not be loaded yet).
  virtual bool HasProfileWithName(std::string_view name) const = 0;

  // Returns whether a profile with `name` can be created.
  virtual bool CanCreateProfileWithName(std::string_view name) const = 0;

  // Reserves a new randomly generated name that can be used to create a new
  // profile and returns the new name. The profile will be registered in the
  // ProfileAttributesStorageIOS and its attributes can be set before the
  // storage on disk is created via CreateProfileAsync() or CreateProfile().
  //
  // After this call, passing the returned value to HasProfileWithName(...)
  // will return true, but passing it to GetProfileWithName(...) will still
  // return a null pointer as the profile has not been created. Loading the
  // profile with LoadProfileAsync() will also fail.
  virtual std::string ReserveNewProfileName() = 0;

  // Returns whether a profile with `name` can be deleted.
  virtual bool CanDeleteProfileWithName(std::string_view name) const = 0;

  // Asynchronously loads a Profile known by `name` if it exists. The callback
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

  // Marks the given Profile for deletion. This must not be called if the
  // profile can not be deleted (for example, personal profile cannot be
  // deleted). Observers will be notified only if the profile is loaded.
  virtual void MarkProfileForDeletion(std::string_view name) = 0;

  // Returns whether the profile with `name` been marked for deletion and
  // still not fully deleted.
  virtual bool IsProfileMarkedForDeletion(std::string_view name) const = 0;

  // Deletes the storage for all profiles marked for deletion (except if
  // they are still loaded) and invokes `callback` when the operation is
  // complete.
  virtual void PurgeProfilesMarkedForDeletion(base::OnceClosure callback) = 0;

  // Returns the ProfileAttributesStorageIOS associated with this manager.
  virtual ProfileAttributesStorageIOS* GetProfileAttributesStorage() = 0;

 protected:
  ProfileManagerIOS() {}

  // Returns a PassKey instance for use by sub-classes.
  static PassKey CreatePassKey() { return PassKey{}; }
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_IOS_H_
