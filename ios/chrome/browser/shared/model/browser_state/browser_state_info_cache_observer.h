// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_OBSERVER_H_

namespace base {
class FilePath;
}

// Observes changes in BrowserStateInfoCache.
class BrowserStateInfoCacheObserver {
 public:
  BrowserStateInfoCacheObserver() {}

  BrowserStateInfoCacheObserver(const BrowserStateInfoCacheObserver&) = delete;
  BrowserStateInfoCacheObserver& operator=(
      const BrowserStateInfoCacheObserver&) = delete;

  virtual ~BrowserStateInfoCacheObserver() {}

  // Called when a BrowserState has been added.
  virtual void OnBrowserStateAdded(const base::FilePath& path) = 0;

  // Called when a BrowserState has been removed.
  virtual void OnBrowserStateWasRemoved(const base::FilePath& path) = 0;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_OBSERVER_H_
