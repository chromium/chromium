// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_OBSERVER_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_OBSERVER_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace base {
class FilePath;
}

// Observes changes in BrowserStateInfoCache.
class BrowserStateInfoCacheObserver {
 public:
  BrowserStateInfoCacheObserver() {}
  virtual ~BrowserStateInfoCacheObserver() {}

  // Called when a BrowserState has been added.
  virtual void OnBrowserStateAdded(const base::FilePath& path) = 0;

  // Called when a BrowserState has been removed.
  virtual void OnBrowserStateWasRemoved(const base::FilePath& path) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserStateInfoCacheObserver);
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_OBSERVER_H_
