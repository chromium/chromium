// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_OBSERVER_H_

#include "base/observer_list_types.h"

class Browser;
class BrowserList;

// Observer interface for BrowserList.
class BrowserListObserver : public base::CheckedObserver {
 public:
  BrowserListObserver() = default;

  BrowserListObserver(const BrowserListObserver&) = delete;
  BrowserListObserver& operator=(const BrowserListObserver&) = delete;

  ~BrowserListObserver() override;

  // Called after `browser` is added to `browser_list`.
  virtual void OnBrowserAdded(const BrowserList* browser_list,
                              Browser* browser) {}

  // Called *after* `browser` is removed from `browser_list`. This method will
  // execute before the object that owns `browser` destroys it, so the pointer
  // passed here will be valid for these method calls, but it can't be used for
  // any processing outside of the synchronous scope of these methods.
  virtual void OnBrowserRemoved(const BrowserList* browser_list,
                                Browser* browser) {}

  // Called before the browserlist is destroyed, in case the observer needs to
  // do any cleanup. After this method is called, all observers will be removed
  // from `browser_list`, and no further observer methods will be called.
  virtual void OnBrowserListShutdown(BrowserList* browser_list) {}
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_OBSERVER_H_
