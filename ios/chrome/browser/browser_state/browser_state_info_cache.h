// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/values.h"

class BrowserStateInfoCacheObserver;
class PrefRegistrySimple;
class PrefService;

// This class saves various information about browser states to local
// preferences.
class BrowserStateInfoCache {
 public:
  BrowserStateInfoCache(PrefService* prefs,
                        const base::FilePath& user_data_dir);

  BrowserStateInfoCache(const BrowserStateInfoCache&) = delete;
  BrowserStateInfoCache& operator=(const BrowserStateInfoCache&) = delete;

  virtual ~BrowserStateInfoCache();

  void AddBrowserState(const base::FilePath& browser_state_path,
                       const std::string& gaia_id,
                       const std::u16string& user_name);
  void RemoveBrowserState(const base::FilePath& browser_state_path);

  // Returns the count of known browser states.
  size_t GetNumberOfBrowserStates() const;

  // Adds and removes an observer.
  void AddObserver(BrowserStateInfoCacheObserver* observer);
  void RemoveObserver(BrowserStateInfoCacheObserver* observer);

  // Gets and sets information related to browser states.
  size_t GetIndexOfBrowserStateWithPath(
      const base::FilePath& browser_state_path) const;
  std::u16string GetUserNameOfBrowserStateAtIndex(size_t index) const;
  base::FilePath GetPathOfBrowserStateAtIndex(size_t index) const;
  std::string GetGAIAIdOfBrowserStateAtIndex(size_t index) const;
  bool BrowserStateIsAuthenticatedAtIndex(size_t index) const;
  void SetAuthInfoOfBrowserStateAtIndex(size_t index,
                                        const std::string& gaia_id,
                                        const std::u16string& user_name);
  void SetBrowserStateIsAuthErrorAtIndex(size_t index, bool value);
  bool BrowserStateIsAuthErrorAtIndex(size_t index) const;

  const base::FilePath& GetUserDataDir() const;

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  const base::Value::Dict* GetInfoForBrowserStateAtIndex(size_t index) const;
  // Saves the browser state info to a cache.
  void SetInfoForBrowserStateAtIndex(size_t index, base::Value::Dict info);

  std::string CacheKeyFromBrowserStatePath(
      const base::FilePath& browser_state_path) const;
  void AddBrowserStateCacheKey(const std::string& key);

  PrefService* prefs_;
  std::vector<std::string> sorted_keys_;
  base::FilePath user_data_dir_;
  base::ObserverList<BrowserStateInfoCacheObserver, true>::Unchecked
      observer_list_;
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_H_
