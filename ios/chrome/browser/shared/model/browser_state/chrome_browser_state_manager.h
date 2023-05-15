// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_CHROME_BROWSER_STATE_MANAGER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_CHROME_BROWSER_STATE_MANAGER_H_

#include <vector>

namespace base {
class FilePath;
}

class BrowserStateInfoCache;
class ChromeBrowserState;

namespace ios {
// Provides methods that allow for various ways of creating non-incognito
// ChromeBrowserState instances. Owns all instances that it creates.
class ChromeBrowserStateManager {
 public:
  ChromeBrowserStateManager(const ChromeBrowserStateManager&) = delete;
  ChromeBrowserStateManager& operator=(const ChromeBrowserStateManager&) =
      delete;

  virtual ~ChromeBrowserStateManager() {}

  // Returns the ChromeBrowserState that was last used, creating one if
  // necessary.
  virtual ChromeBrowserState* GetLastUsedBrowserState() = 0;

  // Returns the ChromeBrowserState associated with `path`, creating one if
  // necessary.
  virtual ChromeBrowserState* GetBrowserState(const base::FilePath& path) = 0;

  // Returns the BrowserStateInfoCache associated with this manager.
  virtual BrowserStateInfoCache* GetBrowserStateInfoCache() = 0;

  // Returns the list of loaded ChromeBrowserStates.
  virtual std::vector<ChromeBrowserState*> GetLoadedBrowserStates() = 0;

 protected:
  ChromeBrowserStateManager() {}
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_CHROME_BROWSER_STATE_MANAGER_H_
