// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_MUTABLE_PROFILE_ATTRIBUTES_STORAGE_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_MUTABLE_PROFILE_ATTRIBUTES_STORAGE_IOS_H_

#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"

// A sub-class of ProfileAttributesStorageIOS that allow creating or deleting
// profiles (its use is restricted to ProfileManagerIOS implementations).
class MutableProfileAttributesStorageIOS : public ProfileAttributesStorageIOS {
 public:
  explicit MutableProfileAttributesStorageIOS(PrefService* prefs);

  ~MutableProfileAttributesStorageIOS();

  // Register profile with `name`. No profile with that name must be registered
  // yet.
  void AddProfile(std::string_view name);

  // Remove information about profile with `name`. A profile with that name
  // must be registered (and won't be anymore once this method returns).
  void RemoveProfile(std::string_view name);
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_MUTABLE_PROFILE_ATTRIBUTES_STORAGE_IOS_H_
