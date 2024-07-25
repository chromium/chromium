// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <vector>

#import "base/files/file_path.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

TestChromeBrowserStateManager::TestChromeBrowserStateManager(
    const base::FilePath& user_data_dir)
    : TestChromeBrowserStateManager(nullptr, user_data_dir) {}

TestChromeBrowserStateManager::TestChromeBrowserStateManager(
    std::unique_ptr<ChromeBrowserState> browser_state)
    : TestChromeBrowserStateManager(std::move(browser_state),
                                    base::FilePath()) {}

TestChromeBrowserStateManager::TestChromeBrowserStateManager(
    std::unique_ptr<ChromeBrowserState> browser_state,
    const base::FilePath& user_data_dir)
    : browser_state_info_cache_(GetApplicationContext()->GetLocalState()) {
  if (browser_state) {
    browser_state_info_cache_.AddBrowserState(
        browser_state->GetBrowserStateName(),
        /*gaia_id=*/std::string(),
        /*user_name=*/std::string());
    last_used_browser_state_path_ = browser_state->GetStatePath();
    browser_states_[browser_state->GetStatePath()] = std::move(browser_state);
  }
}

TestChromeBrowserStateManager::~TestChromeBrowserStateManager() {}

ChromeBrowserState*
TestChromeBrowserStateManager::GetLastUsedBrowserStateDeprecatedDoNotUse() {
  return GetLastUsedBrowserStateForTesting();
}

ChromeBrowserState* TestChromeBrowserStateManager::GetBrowserStateByName(
    std::string_view name) {
  for (auto& pair : browser_states_) {
    if (pair.first.BaseName().AsUTF8Unsafe() == name) {
      return pair.second.get();
    }
  }
  return nullptr;
}

ChromeBrowserState* TestChromeBrowserStateManager::GetBrowserStateByPath(
    const base::FilePath& path) {
  if (!browser_states_[path].get()) {
    browser_states_[path] = TestChromeBrowserState::Builder().Build();
  }
  return browser_states_[path].get();
}

void TestChromeBrowserStateManager::AddBrowserState(
    std::unique_ptr<ChromeBrowserState> state,
    const base::FilePath& path) {
  browser_states_[path] = std::move(state);
}

BrowserStateInfoCache*
TestChromeBrowserStateManager::GetBrowserStateInfoCache() {
  return &browser_state_info_cache_;
}

std::vector<ChromeBrowserState*>
TestChromeBrowserStateManager::GetLoadedBrowserStates() {
  std::vector<ChromeBrowserState*> result;
  for (auto& browser_state : browser_states_) {
    result.push_back(browser_states_[browser_state.first].get());
  }
  return result;
}

void TestChromeBrowserStateManager::LoadBrowserStates() {}

ChromeBrowserState*
TestChromeBrowserStateManager::GetLastUsedBrowserStateForTesting() {
  return browser_states_[last_used_browser_state_path_].get();
}
