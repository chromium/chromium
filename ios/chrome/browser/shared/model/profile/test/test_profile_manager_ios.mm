// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"

#import <vector>

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/test/test_file_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/testing_application_context.h"

TestProfileManagerIOS::TestProfileManagerIOS()
    : profile_attributes_storage_(GetApplicationContext()->GetLocalState()),
      profile_data_dir_(base::CreateUniqueTempDirectoryScopedToTest()) {
  CHECK_EQ(GetApplicationContext()->GetProfileManager(), nullptr);
  TestingApplicationContext::GetGlobal()->SetProfileManager(this);
}

TestProfileManagerIOS::~TestProfileManagerIOS() {
  CHECK_EQ(GetApplicationContext()->GetProfileManager(), this);

  // Notify observers before unregistering from ApplicationContext.
  for (auto& observer : observers_) {
    observer.OnProfileManagerDestroyed(this);
  }

  TestingApplicationContext::GetGlobal()->SetProfileManager(nullptr);
}

void TestProfileManagerIOS::AddObserver(ProfileManagerObserverIOS* observer) {
  observers_.AddObserver(observer);

  for (auto& [key, profile] : profiles_map_) {
    observer->OnProfileCreated(this, profile.get());
    observer->OnProfileLoaded(this, profile.get());
  }
}

void TestProfileManagerIOS::RemoveObserver(
    ProfileManagerObserverIOS* observer) {
  observers_.RemoveObserver(observer);
}

void TestProfileManagerIOS::LoadProfiles() {}

ProfileIOS* TestProfileManagerIOS::GetProfileWithName(std::string_view name) {
  auto iterator = profiles_map_.find(name);
  return iterator != profiles_map_.end() ? iterator->second.get() : nullptr;
}

std::vector<ProfileIOS*> TestProfileManagerIOS::GetLoadedProfiles() {
  std::vector<ProfileIOS*> loaded_profiles;
  for (auto& [name, profile] : profiles_map_) {
    loaded_profiles.push_back(profile.get());
  }
  return loaded_profiles;
}

bool TestProfileManagerIOS::LoadProfileAsync(
    std::string_view name,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  return CreateProfileAsync(name, std::move(initialized_callback),
                            std::move(created_callback));
}

bool TestProfileManagerIOS::CreateProfileAsync(
    std::string_view name,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  auto iterator = profiles_map_.find(name);
  if (iterator == profiles_map_.end()) {
    // Creation is not supported by TestProfileManagerIOS.
    return false;
  }

  ProfileIOS* profile = iterator->second.get();
  if (!created_callback.is_null()) {
    std::move(created_callback).Run(profile);
  }

  if (!initialized_callback.is_null()) {
    std::move(initialized_callback).Run(profile);
  }

  return true;
}

ProfileIOS* TestProfileManagerIOS::LoadProfile(std::string_view name) {
  // TestProfileManagerIOS cannot create nor load a Profile, so the
  // implementation is equivalent to GetProfileWithName(...).
  return GetProfileWithName(name);
}

ProfileIOS* TestProfileManagerIOS::CreateProfile(std::string_view name) {
  // TestProfileManagerIOS cannot create nor load a Profile, so the/
  // implementation is equivalent to GetProfileWithName(...).
  return GetProfileWithName(name);
}

ProfileAttributesStorageIOS*
TestProfileManagerIOS::GetProfileAttributesStorage() {
  return &profile_attributes_storage_;
}

TestProfileIOS* TestProfileManagerIOS::AddProfileWithBuilder(
    TestProfileIOS::Builder builder) {
  // Ensure that the created Profile will store its data in sub-directory of
  // `profile_data_dir_` (i.e. GetStatePath().DirName() == `profile_data_dir_`).
  auto profile = std::move(builder).Build(profile_data_dir_);

  const std::string profile_name = profile->GetProfileName();
  auto [iterator, insertion_success] =
      profiles_map_.insert(std::make_pair(profile_name, std::move(profile)));
  DCHECK(insertion_success);

  profile_attributes_storage_.AddProfile(profile_name);

  for (auto& observer : observers_) {
    observer.OnProfileCreated(this, iterator->second.get());
  }

  for (auto& observer : observers_) {
    observer.OnProfileLoaded(this, iterator->second.get());
  }

  return iterator->second.get();
}
