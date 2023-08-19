// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_MANAGER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_MANAGER_H_

#include <memory>

#include "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
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

  // Adds a browser state to the list of browsers to track.
  void AddBrowserState(std::unique_ptr<ChromeBrowserState>,
                       const base::FilePath& path);

 private:
  TestChromeBrowserStateManager(
      std::unique_ptr<ChromeBrowserState> browser_state,
      const base::FilePath& user_data_dir);

  // The path of the browser state associated with this manager as defined in
  // the constructor.
  base::FilePath last_used_browser_state_path_;

  IOSChromeScopedTestingLocalState local_state_;
  std::map<base::FilePath, std::unique_ptr<ChromeBrowserState>> browser_states_;
  BrowserStateInfoCache browser_state_info_cache_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_MANAGER_H_
