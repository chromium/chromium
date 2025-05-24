// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_OBSERVER_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_OBSERVER_IOS_H_

#import "base/observer_list_types.h"

class ProfileIOS;
class ProfileManagerIOS;

// An observer that can be registered with a ProfileManagerIOS.
class ProfileManagerObserverIOS : public base::CheckedObserver {
 public:
  // Called before the ProfileManagerIOS is destroyed. The observer must
  // drop any ScopedProfileKeepAliveIOS that it owns (or propagates the
  // event if the ScopedProfileKeepAliveIOS is owned indirectly). The
  // application will terminate if any ScopedProfileKeepAliveIOS is not
  // destroyed after this method is called.
  virtual void OnProfileManagerWillBeDestroyed(ProfileManagerIOS* manager) = 0;

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

  // Called when a Profile will be unloaded. This can happen during the app
  // shutdown or when it is determined that the Profile is not needed at the
  // time.
  virtual void OnProfileUnloaded(ProfileManagerIOS* manager,
                                 ProfileIOS* profile) = 0;

  // Called after the user has confirmed they want to delete all data for the
  // Profile. If it is loaded at the time, it will also be unloaded and will
  // no longer be possible to load it anymore.
  //
  // The data is not deleted until the profile has been fully unloaded (it
  // may be delayed to the next application restart if necessary). This is an
  // irreversible operation.
  //
  // OnProfileUnloaded(...) will be called soon after this method.
  virtual void OnProfileMarkedForPermanentDeletion(ProfileManagerIOS* manager,
                                                   ProfileIOS* profile) = 0;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_OBSERVER_IOS_H_
