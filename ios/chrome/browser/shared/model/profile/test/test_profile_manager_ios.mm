// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"

#import <vector>

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/test/test_file_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/test/testing_application_context.h"

TestProfileManagerIOS::TestProfileManagerIOS()
    : profile_attributes_storage_(GetApplicationContext()->GetLocalState()),
      profile_data_dir_(base::CreateUniqueTempDirectoryScopedToTest()) {
  CHECK_EQ(GetApplicationContext()->GetProfileManager(), nullptr);
  TestingApplicationContext* app_context =
      TestingApplicationContext::GetGlobal();
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      app_context->GetSystemIdentityManager(), this);
  app_context->SetProfileManagerAndAccountProfileMapper(
      this, account_profile_mapper_.get());
}

TestProfileManagerIOS::~TestProfileManagerIOS() {
  CHECK_EQ(GetApplicationContext()->GetProfileManager(), this);

  // Notify observers before unregistering from ApplicationContext.
  for (auto& observer : observers_) {
    observer.OnProfileManagerDestroyed(this);
  }

  // The profiles must be unloaded before the AccountProfileMapper is removed
  // from the ApplicationContext, since some keyed services (owned by the
  // profiles) might access the AccountProfileMapper during their destruction.
  UnloadAllProfiles();

  TestingApplicationContext* app_context =
      TestingApplicationContext::GetGlobal();
  app_context->SetProfileManagerAndAccountProfileMapper(nullptr, nullptr);
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

ProfileIOS* TestProfileManagerIOS::GetProfileWithName(std::string_view name) {
  auto iterator = profiles_map_.find(name);
  return iterator != profiles_map_.end() ? iterator->second.get() : nullptr;
}

std::vector<ProfileIOS*> TestProfileManagerIOS::GetLoadedProfiles() const {
  std::vector<ProfileIOS*> loaded_profiles;
  for (auto& [name, profile] : profiles_map_) {
    loaded_profiles.push_back(profile.get());
  }
  return loaded_profiles;
}

bool TestProfileManagerIOS::HasProfileWithName(std::string_view name) const {
  return profile_attributes_storage_.HasProfileWithName(name);
}

bool TestProfileManagerIOS::CanCreateProfileWithName(
    std::string_view name) const {
  return !HasProfileWithName(name);
}

std::string TestProfileManagerIOS::ReserveNewProfileName() {
  std::string name = base::Uuid::GenerateRandomV4().AsLowercaseString();
  CHECK(CanCreateProfileWithName(name));
  profile_attributes_storage_.AddProfile(name);
  return name;
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
  // TestProfileManagerIOS cannot create nor load a Profile, so the
  // implementation is equivalent to GetProfileWithName(...).
  return GetProfileWithName(name);
}

void TestProfileManagerIOS::UnloadProfile(std::string_view name) {
  auto iter = profiles_map_.find(name);
  DCHECK(iter != profiles_map_.end());
  std::unique_ptr<ProfileIOS> profile = std::move(iter->second);
  profiles_map_.erase(iter);
  for (auto& observer : observers_) {
    observer.OnProfileUnloaded(this, profile.get());
  }
}

void TestProfileManagerIOS::UnloadAllProfiles() {
  ProfileMap profiles_map = std::exchange(profiles_map_, {});
  for (auto& [_, profile] : profiles_map) {
    for (auto& observer : observers_) {
      observer.OnProfileUnloaded(this, profile.get());
    }
  }
}

ProfileAttributesStorageIOS*
TestProfileManagerIOS::GetProfileAttributesStorage() {
  return &profile_attributes_storage_;
}

TestProfileIOS* TestProfileManagerIOS::AddProfileWithBuilder(
    TestProfileIOS::Builder builder) {
  const std::string profile_name = builder.GetEffectiveName();
  if (profile_attributes_storage_.HasProfileWithName(profile_name)) {
    CHECK(profile_attributes_storage_
              .GetAttributesForProfileWithName(profile_name)
              .IsNewProfile());
  } else {
    // The ProfileAttributesStorage entry needs to be created before the actual
    // profile initialization gets kicked off, because the AccountProfileMapper
    // depends on it.
    profile_attributes_storage_.AddProfile(profile_name);
  }

  // If this is the first profile ever loaded, mark it as the personal profile.
  if (profile_attributes_storage_.GetPersonalProfileName().empty()) {
    profile_attributes_storage_.SetPersonalProfileName(
        builder.GetEffectiveName());
  }

  // Ensure that the created Profile will store its data in sub-directory of
  // `profile_data_dir_` (i.e. GetStatePath().DirName() == `profile_data_dir_`).
  auto profile = std::move(builder).Build(profile_data_dir_);

  auto [iterator, insertion_success] =
      profiles_map_.insert(std::make_pair(profile_name, std::move(profile)));
  DCHECK(insertion_success);

  for (auto& observer : observers_) {
    observer.OnProfileCreated(this, iterator->second.get());
  }

  // Before notifying observers that the profile was loaded, mark it as
  // no-longer-new.
  profile_attributes_storage_.UpdateAttributesForProfileWithName(
      profile_name, base::BindOnce([](ProfileAttributesIOS attrs) {
        attrs.ClearIsNewProfile();
        return attrs;
      }));

  for (auto& observer : observers_) {
    observer.OnProfileLoaded(this, iterator->second.get());
  }

  return iterator->second.get();
}
