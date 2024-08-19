// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_MANAGER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_MANAGER_H_

#include <map>
#include <memory>

#include "base/observer_list.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"

// ChromeBrowserStateManager implementation for tests.
//
// Register itself with the TestApplicationContext on creation. Requires
// the ApplicationContext's local State to be created before this object.
class TestChromeBrowserStateManager : public ChromeBrowserStateManager {
 public:
  TestChromeBrowserStateManager();

  TestChromeBrowserStateManager(const TestChromeBrowserStateManager&) = delete;
  TestChromeBrowserStateManager& operator=(
      const TestChromeBrowserStateManager&) = delete;

  ~TestChromeBrowserStateManager() override;

  // ChromeBrowserStateManager:
  void AddObserver(ChromeBrowserStateManagerObserver* observer) override;
  void RemoveObserver(ChromeBrowserStateManagerObserver* observer) override;
  void LoadBrowserStates() override;
  ChromeBrowserState* GetLastUsedBrowserStateDeprecatedDoNotUse() override;
  ChromeBrowserState* GetBrowserStateByName(std::string_view name) override;
  std::vector<ChromeBrowserState*> GetLoadedBrowserStates() override;
  bool LoadBrowserStateAsync(
      std::string_view name,
      ChromeBrowserStateLoadedCallback initialized_callback,
      ChromeBrowserStateLoadedCallback created_callback) override;
  bool CreateBrowserStateAsync(
      std::string_view name,
      ChromeBrowserStateLoadedCallback initialized_callback,
      ChromeBrowserStateLoadedCallback created_callback) override;
  ChromeBrowserState* LoadBrowserState(std::string_view name) override;
  ChromeBrowserState* CreateBrowserState(std::string_view name) override;
  BrowserStateInfoCache* GetBrowserStateInfoCache() override;

  // Builds and adds a TestChromeBrowserState using `builder`. Asserts that
  // no BrowserState share the same name. Returns a pointer to the new object.
  TestChromeBrowserState* AddBrowserStateWithBuilder(
      TestChromeBrowserState::Builder builder);

 private:
  // The BrowserStateInfoCache owned by this instance.
  BrowserStateInfoCache browser_state_info_cache_;

  // The path in which the ChromeBrowserStates are stored.
  const base::FilePath data_dir_;

  // The name of the last used ChromeBrowserState (i.e. the first registered).
  std::string last_used_browser_state_name_;

  // Mapping of name to TestChromeBrowserState instances.
  std::map<std::string, std::unique_ptr<TestChromeBrowserState>, std::less<>>
      browser_states_;

  // The list of registered observers.
  base::ObserverList<ChromeBrowserStateManagerObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_MANAGER_H_
