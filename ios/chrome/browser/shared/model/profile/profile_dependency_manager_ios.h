// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_DEPENDENCY_MANAGER_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_DEPENDENCY_MANAGER_IOS_H_

#include "base/types/pass_key.h"
#include "components/keyed_service/core/dependency_manager.h"

class ProfileIOS;

namespace user_prefs {
class PrefRegistrySyncable;
}

// A singleton that manages the KeyedService lifetime taking care of
// creating and destroying them according in order to respect their
// declared dependencies when a Profile is respectively created or
// destroyed.
class ProfileDependencyManagerIOS final : public DependencyManager {
 private:
  // Allow to make the constructor public (for base::NoDestructor<T>)
  // without allowing creation of instances by anyone.
  using PassKey = base::PassKey<ProfileDependencyManagerIOS>;

 public:
  // Returns the singleton instance.
  static ProfileDependencyManagerIOS* GetInstance();

  explicit ProfileDependencyManagerIOS(PassKey pass_key);
  ~ProfileDependencyManagerIOS() final;

  // Register Profile-specific preferences for all services via `registry`.
  void RegisterProfilePrefsForServices(
      user_prefs::PrefRegistrySyncable* registry);

  // Called by each Profile to alert the ProfileDependencyManagerIOS
  // of its creation. Service that want to be started when a Profile
  // is created should pass ServiceCreation::kCreateWithProfile to
  // the (RefCounterd)ProfileKeyedServiceFactoryIOS constructor.
  void CreateProfileServices(ProfileIOS* profile);

  // Similar to CreateProfileServices() except this is used for test
  // Profiles. If TestingCreation::kNoServiceForTests is passed to
  // the (Refcounted)ProfileKeyedServiceFactoryIOS constructor, then
  // the services won't be created for those test Profiles.
  void CreateProfileServicesForTest(ProfileIOS* profile);

  // Called by each Profile to alert the ProfileDependencyManagerIOS
  // of its destruction. This will destroy all services associated
  // with the profile.
  void DestroyProfileServices(ProfileIOS* profile);

  // Marks `profile` as live (i.e. not stale). This method is used as
  // a safeguard against `AssertContextWasntDestroyed(...)` method of
  // DependencyManager going of due to a profile aliasing a ProfileIO
  // from a prior construction (e.g. a test ProfileIOS is created,
  // destroyed and then a new instance created at the same address).
  void MarkProfileLive(ProfileIOS* profile);

 private:
  // Helper function used by CreateProfileServices[ForTest].
  void DoCreateProfileServices(ProfileIOS* profile, bool is_testing_profile);
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_DEPENDENCY_MANAGER_IOS_H_
