// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_ACCESS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_ACCESS_H_

#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

// Used for Typed{Refcounted,}ProfileKeyedServiceFactoryIOS that want to
// support ServiceAccessType and reject incognito profile if the access
// is IMPLICIT_ACCESS.
class RejectImplicitAccessIfIncognito {
 public:
  static inline bool Reject(const ProfileIOS& profile) {
    return profile.IsOffTheRecord();
  }
};

// Used for Typed{Refcounted,}ProfileKeyedServiceFactoryIOS to combine
// multiple access checker types.
template <typename Trait, typename... Rest>
class ServiceAccessChecker {
 public:
  static inline bool Reject(const ProfileIOS& profile) {
    if (Trait::Reject(profile)) {
      return true;
    }

    return ServiceAccessChecker<Rest...>::Reject(profile);
  }
};

// Specialisation of ServiceAccessChecker<...> for the terminal case
// of just one type (it is an error to call this template with zero
// parameters).
template <typename Trait>
class ServiceAccessChecker<Trait> {
 public:
  static inline bool Reject(const ProfileIOS& profile) {
    return Trait::Reject(profile);
  }
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_ACCESS_H_
