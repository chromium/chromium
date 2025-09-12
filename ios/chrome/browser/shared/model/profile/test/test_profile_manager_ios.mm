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
#import "ios/chrome/browser/shared/model/profile/scoped_profile_keep_alive_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/test/testing_application_context.h"

TestProfileManagerIOS::TestProfileManagerIOS()
    : profile_data_dir_(base::CreateUniqueTempDirectoryScopedToTest()) {
  CHECK(GetApplicationContext()->GetLocalState())
      << "The LocalState PrefService must exist! You probably want to "
         "instantiate an IOSChromeScopedTestingLocalState before this class.";
  CHECK_EQ(GetApplicationContext()->GetProfileManager(), nullptr);

  profile_attributes_storage_ =
      std::make_unique<MutableProfileAttributesStorageIOS>(
          GetApplicationContext()->GetLocalState());

  TestingApplicationContext* app_context =
      TestingApplicationContext::GetGlobal();
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      app_context->GetSystemIdentityManager(), this,
      GetApplicationContext()->GetLocalState());
  app_context->SetProfileManagerAndAccountProfileMapper(
      this, account_profile_mapper_.get());
}

TestProfileManagerIOS::~TestProfileManagerIOS() {
  CHECK_EQ(GetApplicationContext()->GetProfileManager(), this);

  // Notify observers before unregistering from ApplicationContext.
  for (auto& observer : observers_) {
    observer.OnProfileManagerDestroyed(this);
  }

  // Unload all the profiles. This ensure that all their KeyedServices
  // (which may be using the AccountProfileMapper) are destroyed before
  // the AccountProfileMapper becomes unaccessible.
  ProfileMap profiles_map = std::exchange(profiles_map_, {});
  for (auto& [_, profile] : profiles_map) {
    for (auto& observer : observers_) {
      observer.OnProfileUnloaded(this, profile.get());
    }
  }

  TestingApplicationContext* app_context =
      TestingApplicationContext::GetGlobal();
  app_context->SetProfileManagerAndAccountProfileMapper(nullptr, nullptr);
}

void TestProfileManagerIOS::PrepareForDestruction() {
  for (auto& observer : observers_) {
    observer.OnProfileManagerWillBeDestroyed(this);
  }
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
  return profile_attributes_storage_->HasProfileWithName(name);
}

bool TestProfileManagerIOS::CanCreateProfileWithName(
    std::string_view name) const {
  return !HasProfileWithName(name);
}

std::string TestProfileManagerIOS::ReserveNewProfileName() {
  return profile_attributes_storage_->ReserveNewProfileName();
}

bool TestProfileManagerIOS::CanDeleteProfileWithName(
    std::string_view name) const {
  return profile_attributes_storage_->CanDeleteProfileWithName(name);
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
    std::move(created_callback).Run(CreateScopedProfileKeepAlive(profile));
  }

  if (!initialized_callback.is_null()) {
    std::move(initialized_callback).Run(CreateScopedProfileKeepAlive(profile));
  }

  return true;
}

void TestProfileManagerIOS::MarkProfileForDeletion(std::string_view name) {
  profile_attributes_storage_->MarkProfileForDeletion(name);

  // If the profile is not loaded, return.
  auto iter = profiles_map_.find(name);
  if (iter == profiles_map_.end()) {
    return;
  }

  TestProfileIOS* profile = iter->second.get();
  for (auto& observer : observers_) {
    observer.OnProfileMarkedForPermanentDeletion(this, profile);
  }
}

bool TestProfileManagerIOS::IsProfileMarkedForDeletion(
    std::string_view name) const {
  return false;
}

void TestProfileManagerIOS::PurgeProfilesMarkedForDeletion(
    base::OnceClosure callback) {
  NOTREACHED();
}

ProfileAttributesStorageIOS*
TestProfileManagerIOS::GetProfileAttributesStorage() {
  return profile_attributes_storage_.get();
}

base::FilePath TestProfileManagerIOS::GetProfilePath(std::string_view name) {
  CHECK(profile_attributes_storage_->HasProfileWithName(name));
  return profile_data_dir_.Append(name);
}

TestProfileIOS* TestProfileManagerIOS::AddProfileWithBuilder(
    TestProfileIOS::Builder builder) {
  const std::string profile_name = builder.GetEffectiveName();
  if (profile_attributes_storage_->HasProfileWithName(profile_name)) {
    CHECK(profile_attributes_storage_
              ->GetAttributesForProfileWithName(profile_name)
              .IsNewProfile());
  } else {
    // The ProfileAttributesStorage entry needs to be created before the actual
    // profile initialization gets kicked off, because the AccountProfileMapper
    // depends on it.
    profile_attributes_storage_->AddProfile(profile_name);
  }

  // If this is the first profile ever loaded, mark it as the personal profile.
  if (profile_attributes_storage_->GetPersonalProfileName().empty()) {
    profile_attributes_storage_->SetPersonalProfileName(
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
  profile_attributes_storage_->UpdateAttributesForProfileWithName(
      profile_name, base::BindOnce([](ProfileAttributesIOS& attrs) {
        attrs.ClearIsNewProfile();
      }));

  for (auto& observer : observers_) {
    observer.OnProfileLoaded(this, iterator->second.get());
  }

  return iterator->second.get();
}

ScopedProfileKeepAliveIOS TestProfileManagerIOS::CreateScopedProfileKeepAlive(
    ProfileIOS* profile) {
  return ScopedProfileKeepAliveIOS(CreatePassKey(), profile, {});
}
