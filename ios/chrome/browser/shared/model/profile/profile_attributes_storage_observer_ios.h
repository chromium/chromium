// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_STORAGE_OBSERVER_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_STORAGE_OBSERVER_IOS_H_

#include <string_view>

#include "base/observer_list_types.h"

// Observer class for ProfileAttributesStorageIOS.
class ProfileAttributesStorageObserverIOS : public base::CheckedObserver {
 public:
  // Called whenever the attributes (as in ProfileAttributesIOS) of the profile
  // named `profile_name` have been updated.
  virtual void OnProfileAttributesUpdated(std::string_view profile_name) {}
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_STORAGE_OBSERVER_IOS_H_
