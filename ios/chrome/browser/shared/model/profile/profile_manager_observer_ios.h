// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_OBSERVER_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_OBSERVER_IOS_H_

#import "base/observer_list_types.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ProfileManagerIOS;

// An observer that can be registered with a ProfileManagerIOS.
class ProfileManagerObserverIOS : public base::CheckedObserver {
 public:
  // Called when the ProfileManagerIOS is destroyed. The observer must
  // unregister itself. This is called as part of the shutdown of the
  // application.
  virtual void OnProfileManagerDestroyed(ProfileManagerIOS* manager) = 0;

  // Called when a Profile is created, before the initialisation is complete. In
  // most case `OnProfileLoaded(...)` is a better event to listen to. Will only
  // be called for non-incognito Profiles.
  //
  // Invoked automatically for all Profiles already created when an observer is
  // registered with the ProfileManagerIOS.
  virtual void OnProfileCreated(ProfileManagerIOS* manager,
                                ProfileIOS* profile) = 0;

  // Called when a Profile has been fully loaded and initialised and is
  // available through the ProfileManagerIOS. Will only be called for
  // non-incognito Profiles.
  //
  // Invoked automatically for all Profiles already loaded when an observer is
  // registered with the ProfileManagerIOS.
  virtual void OnProfileLoaded(ProfileManagerIOS* manager,
                               ProfileIOS* profile) = 0;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_OBSERVER_IOS_H_
