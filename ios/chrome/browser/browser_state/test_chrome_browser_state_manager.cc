// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"

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
    : browser_state_(std::move(browser_state)),
      browser_state_info_cache_(local_state_.Get(),
                                user_data_dir.empty() && browser_state_.get()
                                    ? browser_state_->GetStatePath().DirName()
                                    : user_data_dir) {
  if (browser_state_) {
    browser_state_info_cache_.AddBrowserState(browser_state_->GetStatePath(),
                                              /*gaia_id=*/std::string(),
                                              /*user_name=*/std::u16string());
  }
}

TestChromeBrowserStateManager::~TestChromeBrowserStateManager() {}

ChromeBrowserState* TestChromeBrowserStateManager::GetLastUsedBrowserState() {
  return browser_state_.get();
}

ChromeBrowserState* TestChromeBrowserStateManager::GetBrowserState(
    const base::FilePath& path) {
  if (browser_state_ && browser_state_->GetStatePath() == path)
    return browser_state_.get();
  return nullptr;
}

BrowserStateInfoCache*
TestChromeBrowserStateManager::GetBrowserStateInfoCache() {
  return &browser_state_info_cache_;
}

std::vector<ChromeBrowserState*>
TestChromeBrowserStateManager::GetLoadedBrowserStates() {
  std::vector<ChromeBrowserState*> result;
  if (browser_state_)
    result.push_back(browser_state_.get());
  return result;
}
