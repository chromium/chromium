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
#include "google_apis/gaia/gaia_id.h"

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
  using GaiaIdSet = std::set<GaiaId, std::less<>>;

  // Represents a set of session ids.
  using SessionIds = std::set<std::string, std::less<>>;

  // Creates a ProfileAttributesIOS for a new profile named `profile_name`.
  static ProfileAttributesIOS CreateNew(std::string_view profile_name);

  // Creates a ProfileAttributesIOS for an existing profile name `profile_name`
  // with attributes from `storage`.
  static ProfileAttributesIOS WithAttrs(std::string_view profile_name,
                                        const base::Value::Dict& storage);

  // Creates a ProfileAttributesIOS for a deleted profile named `profile_name`.
  static ProfileAttributesIOS DeletedProfile(std::string_view profile_name);

  ProfileAttributesIOS(ProfileAttributesIOS&&);
  ProfileAttributesIOS& operator=(ProfileAttributesIOS&&);

  ~ProfileAttributesIOS();

  // Returns the name of the profile (immutable).
  const std::string& GetProfileName() const;

  // IsNewProfile() is true if the profile has been registered with
  // ProfileAttributesStorageIOS, but has never been loaded.
  bool IsNewProfile() const;

  // IsFullyInitialized() is true if the profile has been loaded at least once,
  // and all first-time setup steps have been completed (e.g. for work profiles,
  // this includes signing in the corresponding managed account).
  bool IsFullyInitialized() const;

  // IsDeletedProfile() is true if the profile has been marked for deletion.
  bool IsDeletedProfile() const;

  // Gets information related to the profile.
  GaiaId GetGaiaId() const;
  const std::string& GetUserName() const;
  bool HasAuthenticationError() const;
  GaiaIdSet GetAttachedGaiaIds() const;
  base::Time GetLastActiveTime() const;
  bool IsAuthenticated() const;
  SessionIds GetDiscardedSessions() const;
  const base::Value::Dict* GetNotificationPermissions() const;

  // Sets information related to the profile.
  void ClearIsNewProfile();
  void SetFullyInitialized();
  void SetAuthenticationInfo(const GaiaId& gaia_id, std::string_view user_name);
  void SetHasAuthenticationError(bool value);
  void SetAttachedGaiaIds(const GaiaIdSet& gaia_ids);
  void SetLastActiveTime(base::Time time);
  void SetDiscardedSessions(const SessionIds& session_ids);
  void SetNotificationPermissions(base::Value::Dict permissions);

  // Returns the storage.
  base::Value::Dict GetStorage() &&;

 private:
  // Private constructor, use the static factory methods instead.
  ProfileAttributesIOS(std::string_view profile_name,
                       base::Value::Dict storage);

  std::string profile_name_;
  base::Value::Dict storage_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_ATTRIBUTES_IOS_H_
