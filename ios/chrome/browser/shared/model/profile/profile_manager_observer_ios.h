// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_OBSERVER_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_OBSERVER_IOS_H_

#import "base/observer_list_types.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ProfileManagerIOS;

// TODO(crbug.com/358356195): Remove this forward declaration and typedef when
// no usage of ChromeBrowserStateManagerObserver remains.
class ProfileManagerObserverIOS;
using ChromeBrowserStateManagerObserver = ProfileManagerObserverIOS;

// An observer that can be registered with a ChromeBrowserStateManager.
class ProfileManagerObserverIOS : public base::CheckedObserver {
 public:
  // Called when the ProfileManagerIOS is destroyed. The observer
  // must unregister itself. This is called as part of the shutdown of the
  // application.
  virtual void OnChromeBrowserStateManagerDestroyed(
      ProfileManagerIOS* manager) = 0;

  // Called when a ChromeBrowserState is created, before the initialisation is
  // complete. In most case `OnBrowserStateAdded(...)` is a better event to
  // listen to. Will only be called for non-incognito ChromeBrowserState.
  //
  // Invoked automatically for all ChromeBrowserState already created when
  // an observer is registered with ChromeBrowserStateManager.
  virtual void OnChromeBrowserStateCreated(
      ProfileManagerIOS* manager,
      ChromeBrowserState* browser_state) = 0;

  // Called when a ChromeBrowserState has been fully loaded and initialised and
  // is available through the ProfileManagerIOS. Will only be called for
  // non-incognito ChromeBrowserState.
  //
  // Invoked automatically for all ChromeBrowserState already loaded when
  // an observer is registered with ChromeBrowserStateManager.
  virtual void OnChromeBrowserStateLoaded(
      ProfileManagerIOS* manager,
      ChromeBrowserState* browser_state) = 0;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_OBSERVER_IOS_H_
