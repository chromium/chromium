// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_MODEL_CHROME_BROWSER_STATE_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_MODEL_CHROME_BROWSER_STATE_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"

class PrefService;

// ChromeBrowserStateManager implementation.
class ChromeBrowserStateManagerImpl : public ChromeBrowserStateManager,
                                      public ChromeBrowserState::Delegate {
 public:
  // Constructs the ChromeBrowserStateManagerImpl with a pointer to the local
  // state's PrefService and with the path to the directory containing the
  // ChromeBrowserStates' data.
  ChromeBrowserStateManagerImpl(PrefService* local_state,
                                const base::FilePath& data_dir);

  ChromeBrowserStateManagerImpl(const ChromeBrowserStateManagerImpl&) = delete;
  ChromeBrowserStateManagerImpl& operator=(
      const ChromeBrowserStateManagerImpl&) = delete;

  ~ChromeBrowserStateManagerImpl() override;

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
  class BrowserStateInfo;

  using CreationMode = ChromeBrowserState::CreationMode;
  using ChromeBrowserMap = std::map<std::string, BrowserStateInfo, std::less<>>;

  // Get the name of the last used browser state, or if that's undefined, the
  // default browser state.
  std::string GetLastUsedBrowserStateName() const;

  // Returns whether a ChromeBrowserState with `name` exists on disk.
  bool BrowserStateWithNameExists(std::string_view name);

  // Returns if creating a ChromeBrowserState with `name` is allowed.
  bool CanCreateBrowserStateWithName(std::string_view name);

  // Creates or loads the ChromeBrowserState known by `name` using the
  // `creation_mode`. The callbacks have the same meaning as the method
  // CreateBrowserStateAsync(...). Returns whether a ChromeBrowserState
  // with that name already exists or it can be created.
  bool CreateBrowserStateWithMode(
      std::string_view name,
      CreationMode creation_mode,
      ChromeBrowserStateLoadedCallback initialized_callback,
      ChromeBrowserStateLoadedCallback created_callback);

  // Adds `browser_state` to the browser state info cache if necessary.
  void AddBrowserStateToCache(ChromeBrowserState* browser_state);

  // Final initialization of the browser state.
  void DoFinalInit(ChromeBrowserState* browser_state);
  void DoFinalInitForServices(ChromeBrowserState* browser_state);

  SEQUENCE_CHECKER(sequence_checker_);

  // The PrefService storing the local state.
  raw_ptr<PrefService> local_state_;

  // The path to the directory where the ChromeBrowserStates are stored.
  const base::FilePath data_dir_;

  // Holds the ChromeBrowserState instances that this instance has created.
  ChromeBrowserMap browser_states_;

  // The owned BrowserStateInfoCache instance. Lazily created.
  std::unique_ptr<BrowserStateInfoCache> browser_state_info_cache_;

  // The list of registered observers.
  base::ObserverList<ChromeBrowserStateManagerObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_MODEL_CHROME_BROWSER_STATE_MANAGER_IMPL_H_
