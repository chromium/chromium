// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_IOS_H_

#include <set>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "base/values.h"

// Stores information about a single profile.
//
// It represents a snapshot in time of the data stored in preferences. It
// does not reflect changes made to the preferences and it does not store
// the values to the preferences when changes are made.
//
// Look at ProfileAttributesStorageIOS API to see how the changes can be
// committed to the preferences.
class ProfileAttributesIOS {
 public:
  // Represents a set of gaia ids.
  using GaiaIdSet = std::set<std::string, std::less<>>;

  ProfileAttributesIOS(std::string_view profile_name,
                       const base::Value::Dict* attrs);

  ProfileAttributesIOS(ProfileAttributesIOS&&);
  ProfileAttributesIOS& operator=(ProfileAttributesIOS&&);

  ~ProfileAttributesIOS();

  // Returns the name of the profile (immutable).
  const std::string& GetProfileName() const;

  // Gets information related to the profile.
  const std::string& GetGaiaId() const;
  const std::string& GetUserName() const;
  bool HasAuthenticationError() const;
  GaiaIdSet GetAttachedGaiaIds() const;
  base::Time GetLastActiveTime() const;
  bool IsAuthenticated() const;

  // Sets information related to the profile.
  void SetAuthenticationInfo(std::string_view gaia_id,
                             std::string_view user_name);
  void SetHasAuthenticationError(bool value);
  void SetAttachedGaiaIds(const GaiaIdSet& gaia_ids);
  void SetLastActiveTime(base::Time time);

  // Returns the storage.
  base::Value::Dict GetStorage() &&;

 private:
  std::string profile_name_;
  base::Value::Dict storage_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_IOS_H_
