// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"

#import <vector>

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/test/test_file_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/test/testing_application_context.h"

TestChromeBrowserStateManager::TestChromeBrowserStateManager()
    : browser_state_info_cache_(GetApplicationContext()->GetLocalState()),
      data_dir_(base::CreateUniqueTempDirectoryScopedToTest()) {
  CHECK_EQ(GetApplicationContext()->GetChromeBrowserStateManager(), nullptr);
  TestingApplicationContext::GetGlobal()->SetChromeBrowserStateManager(this);
}

TestChromeBrowserStateManager::~TestChromeBrowserStateManager() {
  CHECK_EQ(GetApplicationContext()->GetChromeBrowserStateManager(), this);

  // Notify observers before unregistering from ApplicationContext.
  for (auto& observer : observers_) {
    observer.OnChromeBrowserStateManagerDestroyed(this);
  }

  TestingApplicationContext::GetGlobal()->SetChromeBrowserStateManager(nullptr);
}

void TestChromeBrowserStateManager::AddObserver(
    ChromeBrowserStateManagerObserver* observer) {
  observers_.AddObserver(observer);

  for (auto& [key, browser_state] : browser_states_) {
    observer->OnChromeBrowserStateCreated(this, browser_state.get());
    observer->OnChromeBrowserStateLoaded(this, browser_state.get());
  }
}

void TestChromeBrowserStateManager::RemoveObserver(
    ChromeBrowserStateManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TestChromeBrowserStateManager::LoadBrowserStates() {}

ChromeBrowserState*
TestChromeBrowserStateManager::GetLastUsedBrowserStateDeprecatedDoNotUse() {
  return GetBrowserStateByName(last_used_browser_state_name_);
}

ChromeBrowserState* TestChromeBrowserStateManager::GetBrowserStateByName(
    std::string_view name) {
  auto iterator = browser_states_.find(name);
  return iterator != browser_states_.end() ? iterator->second.get() : nullptr;
}

std::vector<ChromeBrowserState*>
TestChromeBrowserStateManager::GetLoadedBrowserStates() {
  std::vector<ChromeBrowserState*> result;
  for (auto& browser_state : browser_states_) {
    result.push_back(browser_states_[browser_state.first].get());
  }
  return result;
}

bool TestChromeBrowserStateManager::LoadBrowserStateAsync(
    std::string_view name,
    ChromeBrowserStateLoadedCallback initialized_callback,
    ChromeBrowserStateLoadedCallback created_callback) {
  return CreateBrowserStateAsync(name, std::move(initialized_callback),
                                 std::move(created_callback));
}

bool TestChromeBrowserStateManager::CreateBrowserStateAsync(
    std::string_view name,
    ChromeBrowserStateLoadedCallback initialized_callback,
    ChromeBrowserStateLoadedCallback created_callback) {
  auto iterator = browser_states_.find(name);
  if (iterator == browser_states_.end()) {
    // Creation is not supported by TestChromeBrowserStateManager.
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

ChromeBrowserState* TestChromeBrowserStateManager::LoadBrowserState(
    std::string_view name) {
  // TestChromeBrowserState cannot create nor load a ChromeBrowserState,
  // so the implementation is equivalent to GetBrowserStateByName(...).
  return GetBrowserStateByName(name);
}

ChromeBrowserState* TestChromeBrowserStateManager::CreateBrowserState(
    std::string_view name) {
  // TestChromeBrowserState cannot create nor load a ChromeBrowserState,
  // so the implementation is equivalent to GetBrowserStateByName(...).
  return GetBrowserStateByName(name);
}

BrowserStateInfoCache*
TestChromeBrowserStateManager::GetBrowserStateInfoCache() {
  return &browser_state_info_cache_;
}

TestChromeBrowserState*
TestChromeBrowserStateManager::AddBrowserStateWithBuilder(
    TestChromeBrowserState::Builder builder) {
  // Ensure that the created BrowserState will store its data in sub-directory
  // of `data_dir_` (i.e. GetBrowserStatePath().DirName() will be `data_dir_`).
  auto browser_state = std::move(builder).Build(data_dir_);

  const std::string browser_state_name = browser_state->GetBrowserStateName();
  auto [iterator, insertion_success] = browser_states_.insert(
      std::make_pair(browser_state_name, std::move(browser_state)));
  DCHECK(insertion_success);

  // Notify of the creation of the ChromeBrowserState before updating the
  // BrowserStateInfoCache or the last_used_browser_state_name_ since the
  // observers may observe similar behaviour with the real implementation
  // when the ChromeBrowserState is loaded asynchronously.
  for (auto& observer : observers_) {
    observer.OnChromeBrowserStateCreated(this, iterator->second.get());
  }

  if (last_used_browser_state_name_.empty()) {
    last_used_browser_state_name_ = browser_state_name;
  }

  browser_state_info_cache_.AddBrowserState(browser_state_name,
                                            /*gaia_id=*/std::string(),
                                            /*user_name=*/std::string());

  for (auto& observer : observers_) {
    observer.OnChromeBrowserStateLoaded(this, iterator->second.get());
  }

  return iterator->second.get();
}
