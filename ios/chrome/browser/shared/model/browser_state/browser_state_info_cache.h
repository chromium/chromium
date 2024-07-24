// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_H_

#include <stddef.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"

class BrowserStateInfoCacheObserver;
class PrefRegistrySimple;
class PrefService;

// This class saves various information about browser states to local
// preferences.
class BrowserStateInfoCache {
 public:
  explicit BrowserStateInfoCache(PrefService* prefs);

  BrowserStateInfoCache(const BrowserStateInfoCache&) = delete;
  BrowserStateInfoCache& operator=(const BrowserStateInfoCache&) = delete;

  ~BrowserStateInfoCache();

  void AddBrowserState(std::string_view name,
                       std::string_view gaia_id,
                       std::string_view user_name);
  void RemoveBrowserState(std::string_view name);

  // Returns the count of known browser states.
  size_t GetNumberOfBrowserStates() const;

  // Adds and removes an observer.
  void AddObserver(BrowserStateInfoCacheObserver* observer);
  void RemoveObserver(BrowserStateInfoCacheObserver* observer);

  // Gets and sets information related to browser states.
  size_t GetIndexOfBrowserStateWithName(std::string_view name) const;
  const std::string& GetNameOfBrowserStateAtIndex(size_t index) const;
  const std::string& GetGAIAIdOfBrowserStateAtIndex(size_t index) const;
  const std::string& GetUserNameOfBrowserStateAtIndex(size_t index) const;
  bool BrowserStateIsAuthenticatedAtIndex(size_t index) const;
  void SetAuthInfoOfBrowserStateAtIndex(size_t index,
                                        std::string_view gaia_id,
                                        std::string_view user_name);
  void SetBrowserStateIsAuthErrorAtIndex(size_t index, bool value);
  bool BrowserStateIsAuthErrorAtIndex(size_t index) const;

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Returns the dictionary storing information about a browser state.
  const base::Value::Dict* GetInfoForBrowserStateAtIndex(size_t index) const;

  // Saves the browser state info to a cache.
  void SetInfoForBrowserStateAtIndex(size_t index, base::Value::Dict info);

  raw_ptr<PrefService> prefs_;
  std::vector<std::string> sorted_keys_;
  base::ObserverList<BrowserStateInfoCacheObserver, true> observer_list_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_H_
