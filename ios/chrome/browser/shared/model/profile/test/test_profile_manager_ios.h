// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TEST_PROFILE_MANAGER_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TEST_PROFILE_MANAGER_IOS_H_

#include <map>
#include <memory>

#include "base/observer_list.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"

// ProfileManagerIOS implementation for tests.
//
// Register itself with the TestApplicationContext on creation. Requires
// the ApplicationContext's local State to be created before this object.
class TestProfileManagerIOS : public ProfileManagerIOS {
 public:
  TestProfileManagerIOS();

  TestProfileManagerIOS(const TestProfileManagerIOS&) = delete;
  TestProfileManagerIOS& operator=(const TestProfileManagerIOS&) = delete;

  ~TestProfileManagerIOS() override;

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

  // Builds and adds a TestProfileIOS using `builder`. Asserts that no Profile
  // share the same name. Returns a pointer to the new object.
  TestProfileIOS* AddProfileWithBuilder(TestProfileIOS::Builder builder);

 private:
  // The ProfileAttributesStorageIOS owned by this instance.
  ProfileAttributesStorageIOS profile_attributes_storage_;

  // The path in which the Profiles' data are stored.
  const base::FilePath profile_data_dir_;

  // Mapping of name to TestProfileIOS instances.
  std::map<std::string, std::unique_ptr<TestProfileIOS>, std::less<>>
      profiles_map_;

  // The list of registered observers.
  base::ObserverList<ProfileManagerObserverIOS, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TEST_PROFILE_MANAGER_IOS_H_
