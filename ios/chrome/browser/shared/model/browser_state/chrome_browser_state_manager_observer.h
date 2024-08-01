// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_CHROME_BROWSER_STATE_MANAGER_OBSERVER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_CHROME_BROWSER_STATE_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"

class ChromeBrowserState;
class ChromeBrowserStateManager;

// An observer that can be registered with a ChromeBrowserStateManager.
class ChromeBrowserStateManagerObserver : public base::CheckedObserver {
 public:
  // Called when the ChromeBrowserStateManager is destroyed. The observer
  // must unregister itself. This is called as part of the shutdown of the
  // application.
  virtual void OnChromeBrowserStateManagerDestroyed(
      ChromeBrowserStateManager* manager) = 0;

  // Called when a ChromeBrowserState is created, before the initialisation
  // is complete. In most case `OnBrowserStateAdded(...)` is a better event
  // to listen to. Will only be called for non-incognito ChromeBrowserState.
  virtual void OnChromeBrowserStateCreated(
      ChromeBrowserStateManager* manager,
      ChromeBrowserState* browser_state) = 0;

  // Called when a ChromeBrowserState has been fully loaded and initialised
  // and is available through the ChromeBrowserStateManager. Will only be
  // called for non-incognito ChromeBrowserState.
  virtual void OnChromeBrowserStateLoaded(
      ChromeBrowserStateManager* manager,
      ChromeBrowserState* browser_state) = 0;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_CHROME_BROWSER_STATE_MANAGER_OBSERVER_H_
