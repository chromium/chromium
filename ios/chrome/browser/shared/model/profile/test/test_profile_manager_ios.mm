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
      data_dir_(base::CreateUniqueTempDirectoryScopedToTest()) {
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

  for (auto& [key, browser_state] : browser_states_) {
    observer->OnProfileCreated(this, browser_state.get());
    observer->OnProfileLoaded(this, browser_state.get());
  }
}

void TestProfileManagerIOS::RemoveObserver(
    ProfileManagerObserverIOS* observer) {
  observers_.RemoveObserver(observer);
}

void TestProfileManagerIOS::LoadBrowserStates() {}

ChromeBrowserState*
TestProfileManagerIOS::GetLastUsedBrowserStateDeprecatedDoNotUse() {
  return GetProfileWithName(last_used_browser_state_name_);
}

ChromeBrowserState* TestProfileManagerIOS::GetProfileWithName(
    std::string_view name) {
  auto iterator = browser_states_.find(name);
  return iterator != browser_states_.end() ? iterator->second.get() : nullptr;
}

std::vector<ChromeBrowserState*>
TestProfileManagerIOS::GetLoadedBrowserStates() {
  std::vector<ChromeBrowserState*> result;
  for (auto& browser_state : browser_states_) {
    result.push_back(browser_states_[browser_state.first].get());
  }
  return result;
}

bool TestProfileManagerIOS::LoadBrowserStateAsync(
    std::string_view name,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  return CreateBrowserStateAsync(name, std::move(initialized_callback),
                                 std::move(created_callback));
}

bool TestProfileManagerIOS::CreateBrowserStateAsync(
    std::string_view name,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  auto iterator = browser_states_.find(name);
  if (iterator == browser_states_.end()) {
    // Creation is not supported by TestProfileManagerIOS.
    return false;
  }

  ChromeBrowserState* browser_state = iterator->second.get();
  if (!created_callback.is_null()) {
    std::move(created_callback).Run(browser_state);
  }

  if (!initialized_callback.is_null()) {
    std::move(initialized_callback).Run(browser_state);
  }

  return true;
}

ChromeBrowserState* TestProfileManagerIOS::LoadBrowserState(
    std::string_view name) {
  // TestChromeBrowserState cannot create nor load a ChromeBrowserState,
  // so the implementation is equivalent to GetProfileWithName(...).
  return GetProfileWithName(name);
}

ChromeBrowserState* TestProfileManagerIOS::CreateBrowserState(
    std::string_view name) {
  // TestChromeBrowserState cannot create nor load a ChromeBrowserState,
  // so the implementation is equivalent to GetProfileWithName(...).
  return GetProfileWithName(name);
}

ProfileAttributesStorageIOS*
TestProfileManagerIOS::GetProfileAttributesStorage() {
  return &profile_attributes_storage_;
}

TestChromeBrowserState*
TestProfileManagerIOS::AddBrowserStateWithBuilder(
    TestChromeBrowserState::Builder builder) {
  // Ensure that the created BrowserState will store its data in sub-directory
  // of `data_dir_` (i.e. GetBrowserStatePath().DirName() will be `data_dir_`).
  auto browser_state = std::move(builder).Build(data_dir_);

  const std::string browser_state_name = browser_state->GetBrowserStateName();
  auto [iterator, insertion_success] = browser_states_.insert(
      std::make_pair(browser_state_name, std::move(browser_state)));
  DCHECK(insertion_success);

  profile_attributes_storage_.AddProfile(browser_state_name);
  if (last_used_browser_state_name_.empty()) {
    last_used_browser_state_name_ = browser_state_name;
  }

  for (auto& observer : observers_) {
    observer.OnProfileCreated(this, iterator->second.get());
  }

  for (auto& observer : observers_) {
    observer.OnProfileLoaded(this, iterator->second.get());
  }

  return iterator->second.get();
}
