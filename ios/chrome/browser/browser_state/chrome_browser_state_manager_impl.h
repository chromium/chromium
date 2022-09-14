// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_MANAGER_IMPL_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"

class ChromeBrowserStateImpl;

// ChromeBrowserStateManager implementation.
class ChromeBrowserStateManagerImpl : public ios::ChromeBrowserStateManager {
 public:
  ChromeBrowserStateManagerImpl();

  ChromeBrowserStateManagerImpl(const ChromeBrowserStateManagerImpl&) = delete;
  ChromeBrowserStateManagerImpl& operator=(
      const ChromeBrowserStateManagerImpl&) = delete;

  ~ChromeBrowserStateManagerImpl() override;

  // ChromeBrowserStateManager:
  ChromeBrowserState* GetLastUsedBrowserState() override;
  ChromeBrowserState* GetBrowserState(const base::FilePath& path) override;
  BrowserStateInfoCache* GetBrowserStateInfoCache() override;
  std::vector<ChromeBrowserState*> GetLoadedBrowserStates() override;

 private:
  using ChromeBrowserStateImplPathMap =
      std::map<base::FilePath, std::unique_ptr<ChromeBrowserStateImpl>>;

  // Get the path of the last used browser state, or if that's undefined, the
  // default browser state.
  base::FilePath GetLastUsedBrowserStateDir(
      const base::FilePath& user_data_dir);

  // Final initialization of the browser state.
  void DoFinalInit(ChromeBrowserState* browser_state);
  void DoFinalInitForServices(ChromeBrowserState* browser_state);

  // Adds `browser_state` to the browser state info cache if it hasn't been
  // added yet.
  void AddBrowserStateToCache(ChromeBrowserState* browser_state);

  // Holds the ChromeBrowserStateImpl instances that this instance has created.
  ChromeBrowserStateImplPathMap browser_states_;
  std::unique_ptr<BrowserStateInfoCache> browser_state_info_cache_;
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_MANAGER_IMPL_H_
