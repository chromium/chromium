// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_MODEL_CHROME_BROWSER_STATE_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_MODEL_CHROME_BROWSER_STATE_MANAGER_IMPL_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"

// ChromeBrowserStateManager implementation.
class ChromeBrowserStateManagerImpl : public ios::ChromeBrowserStateManager,
                                      public ChromeBrowserState::Delegate {
 public:
  ChromeBrowserStateManagerImpl();

  ChromeBrowserStateManagerImpl(const ChromeBrowserStateManagerImpl&) = delete;
  ChromeBrowserStateManagerImpl& operator=(
      const ChromeBrowserStateManagerImpl&) = delete;

  ~ChromeBrowserStateManagerImpl() override;

  // ChromeBrowserStateManager:
  ChromeBrowserState* GetLastUsedBrowserStateDeprecatedDoNotUse() override;
  ChromeBrowserState* GetBrowserState(const base::FilePath& path) override;
  BrowserStateInfoCache* GetBrowserStateInfoCache() override;
  std::vector<ChromeBrowserState*> GetLoadedBrowserStates() override;
  void LoadBrowserStates() override;

  // ChromeBrowserState::Delegate:
  void OnChromeBrowserStateCreationStarted(
      ChromeBrowserState* browser_state,
      ChromeBrowserState::CreationMode creation_mode) override;
  void OnChromeBrowserStateCreationFinished(
      ChromeBrowserState* browser_state,
      ChromeBrowserState::CreationMode creation_mode,
      bool is_new_browser_state,
      bool success) override;

 private:
  using ChromeBrowserStatePathMap =
      std::map<base::FilePath, std::unique_ptr<ChromeBrowserState>>;

  // Callback invoked with the BrowserState once its initialisation is done.
  // May be invoked with nullptr if loading the BrowserState failed. Will be
  // called on the calling sequence, but may be asynchronous.
  using BrowserStateLoadedCallback =
      base::OnceCallback<void(ChromeBrowserState*)>;

  // Get the path of the last used browser state, or if that's undefined, the
  // default browser state.
  base::FilePath GetLastUsedBrowserStateDir(
      const base::FilePath& user_data_dir);

  // Load ChromeBrowserState at `path` and invoke `callback` when the load
  // is complete.
  void LoadBrowserState(const base::FilePath& path,
                        BrowserStateLoadedCallback callback);

  // Final initialization of the browser state.
  void DoFinalInit(ChromeBrowserState* browser_state);
  void DoFinalInitForServices(ChromeBrowserState* browser_state);

  // Adds `browser_state` to the browser state info cache if it hasn't been
  // added yet.
  void AddBrowserStateToCache(ChromeBrowserState* browser_state);

  // Holds the ChromeBrowserState instances that this instance has created.
  ChromeBrowserStatePathMap browser_states_;
  std::unique_ptr<BrowserStateInfoCache> browser_state_info_cache_;
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_MODEL_CHROME_BROWSER_STATE_MANAGER_IMPL_H_
