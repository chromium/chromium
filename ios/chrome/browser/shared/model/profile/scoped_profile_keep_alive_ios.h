// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_SCOPED_PROFILE_KEEP_ALIVE_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_SCOPED_PROFILE_KEEP_ALIVE_IOS_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"

class ProfileIOS;
class ProfileManagerIOS;

// Wrapper returned by ProfileManagerIOS when loading a ProfileIOS. As
// long as this object is kept alive, the profile will not be unloaded.
//
// Those objects are created and returned by ProfileManagerIOS as any
// code that wants to extend the lifetime of a ProfileIOS needs to do
// it while respecting the ProfileManagerIOS invariants (one of them
// is that the ProfileIOS must never outlive the ProfileManagerIOS).
//
// To get a new instance of ScopedProfileKeepAliveIOS, use the methods
// LoadProfileAsync(...) or CreateProfileAsync(...) which can be called
// for an already loaded profile.
//
// The ScopedProfileKeepAliveIOS instances must be dropped before the
// ProfileManagerIOS is destroyed. A good way to ensure this happen is
// to implement ProfileManagerObserverIOS API and to reset the objects
// when `OnProfileManagerWillBeDestroyed(...)` is called.
class [[maybe_unused, nodiscard]] ScopedProfileKeepAliveIOS {
 public:
  using Cleanup = base::OnceClosure;
  using PassKey = base::PassKey<ProfileManagerIOS>;

  ScopedProfileKeepAliveIOS();
  ScopedProfileKeepAliveIOS(PassKey, ProfileIOS* profile, Cleanup cleanup);

  // Move-only type.
  ScopedProfileKeepAliveIOS(ScopedProfileKeepAliveIOS&& other);
  ScopedProfileKeepAliveIOS& operator=(ScopedProfileKeepAliveIOS&& other);

  ~ScopedProfileKeepAliveIOS();

  // Returns the profile instance.
  ProfileIOS* profile() { return profile_.get(); }

  // Resets the ScopedProfileKeepAliveIOS, possibly causing the profile
  // to be unloaded (similar as destroying the object).
  void Reset();

 private:
  // The profile instance. May be null if the profile could not be loaded.
  raw_ptr<ProfileIOS> profile_;

  // The cleanup callback. Used to inform of this object destruction.
  Cleanup cleanup_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_SCOPED_PROFILE_KEEP_ALIVE_IOS_H_
