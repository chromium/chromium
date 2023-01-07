// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_MANAGER_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_MANAGER_H_

#include <memory>

#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"

// ChromeBrowserStateManager implementation for tests.
class TestChromeBrowserStateManager : public ios::ChromeBrowserStateManager {
 public:
  explicit TestChromeBrowserStateManager(const base::FilePath& user_data_dir);
  explicit TestChromeBrowserStateManager(
      std::unique_ptr<ChromeBrowserState> browser_state);

  TestChromeBrowserStateManager(const TestChromeBrowserStateManager&) = delete;
  TestChromeBrowserStateManager& operator=(
      const TestChromeBrowserStateManager&) = delete;

  ~TestChromeBrowserStateManager() override;

  // ChromeBrowserStateManager:
  ChromeBrowserState* GetLastUsedBrowserState() override;
  ChromeBrowserState* GetBrowserState(const base::FilePath& path) override;
  BrowserStateInfoCache* GetBrowserStateInfoCache() override;
  std::vector<ChromeBrowserState*> GetLoadedBrowserStates() override;

 private:
  TestChromeBrowserStateManager(
      std::unique_ptr<ChromeBrowserState> browser_state,
      const base::FilePath& user_data_dir);

  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  BrowserStateInfoCache browser_state_info_cache_;
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_MANAGER_H_
