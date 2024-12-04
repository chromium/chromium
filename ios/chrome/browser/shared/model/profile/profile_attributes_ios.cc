// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"

#include "base/check.h"
#include "base/json/values_util.h"
#include "base/strings/string_util.h"

namespace {

using Dict = base::Value::Dict;

// Constants used as key in the dictionary.
constexpr std::string_view kActiveTimeKey = "active_time";
constexpr std::string_view kGaiaIdKey = "gaia_id";
constexpr std::string_view kIsAuthErrorKey = "is_auth_error";
constexpr std::string_view kAttachedGaiaIdsKey = "attached_gaia_ids";
constexpr std::string_view kUserNameKey = "user_name";
constexpr std::string_view kNewProfile = "new_profile";
constexpr std::string_view kDiscardedSessions = "discarded_sessions";

// Retrieves a bool value from the dictionary.
bool GetBool(const Dict& dict, std::string_view key) {
  return dict.FindBool(key).value_or(false);
}

// Stores a bool value in the dictionary.
void SetBool(Dict& dict, std::string_view key, bool value) {
  if (!value) {
    dict.Remove(key);
  } else {
    dict.Set(key, value);
  }
}

// Retrieves a Time value from the dictionary.
base::Time GetTime(const Dict& dict, std::string_view key) {
  return base::ValueToTime(dict.Find(key)).value_or(base::Time());
}

// Stores a Time value in the dictionary.
void SetTime(Dict& dict, std::string_view key, base::Time value) {
  if (value == base::Time()) {
    dict.Remove(key);
  } else {
    dict.Set(key, base::TimeToValue(value));
  }
}

// Retrieves a string value from the dictionary.
const std::string& GetString(const Dict& dict, std::string_view key) {
  const std::string* string = dict.FindString(key);
  return string ? *string : base::EmptyString();
}

// Stores a string value in the dictionary.
void SetString(Dict& dict, std::string_view key, std::string_view value) {
  if (value.empty()) {
    dict.Remove(key);
  } else {
    dict.Set(key, value);
  }
}

// Retrieves a string set value from the dictionary.
template <typename StringSet = std::set<std::string>>
StringSet GetStringSet(const Dict& dict, std::string_view key) {
  StringSet set;
  if (const base::Value::List* list = dict.FindList(key)) {
    for (const base::Value& value : *list) {
      if (const std::string* string = value.GetIfString()) {
        set.insert(*string);
      }
    }
  }
  return set;
}

// Stores a string set value in the dictionary.
template <typename StringSet = std::set<std::string>>
void SetStringSet(Dict& dict, std::string_view key, const StringSet& set) {
  if (set.empty()) {
    dict.Remove(key);
  } else {
    base::Value::List list;
    for (const std::string& string : set) {
      list.Append(string);
    }
    dict.Set(key, std::move(list));
  }
}

}  // namespace

// static
ProfileAttributesIOS ProfileAttributesIOS::CreateNew(
    std::string_view profile_name) {
  base::Value::Dict dict;
  SetBool(dict, kNewProfile, true);
  return ProfileAttributesIOS(profile_name, std::move(dict));
}

// static
ProfileAttributesIOS ProfileAttributesIOS::WithAttrs(
    std::string_view profile_name,
    const base::Value::Dict& storage) {
  return ProfileAttributesIOS(profile_name, storage.Clone());
}

ProfileAttributesIOS::ProfileAttributesIOS(ProfileAttributesIOS&&) = default;

ProfileAttributesIOS& ProfileAttributesIOS::operator=(ProfileAttributesIOS&&) =
    default;

ProfileAttributesIOS::~ProfileAttributesIOS() = default;

const std::string& ProfileAttributesIOS::ProfileAttributesIOS::GetProfileName()
    const {
  return profile_name_;
}

bool ProfileAttributesIOS::IsNewProfile() const {
  return GetBool(storage_, kNewProfile);
}

const std::string& ProfileAttributesIOS::GetGaiaId() const {
  return GetString(storage_, kGaiaIdKey);
}

const std::string& ProfileAttributesIOS::GetUserName() const {
  return GetString(storage_, kUserNameKey);
}

bool ProfileAttributesIOS::HasAuthenticationError() const {
  return GetBool(storage_, kIsAuthErrorKey);
}

ProfileAttributesIOS::GaiaIdSet ProfileAttributesIOS::GetAttachedGaiaIds()
    const {
  return GetStringSet<GaiaIdSet>(storage_, kAttachedGaiaIdsKey);
}

base::Time ProfileAttributesIOS::GetLastActiveTime() const {
  return GetTime(storage_, kActiveTimeKey);
}

bool ProfileAttributesIOS::IsAuthenticated() const {
  // The profile is authenticated if the gaia_id is not empty. If it is empty,
  // check if the username is not empty. This latter check is needed in case
  // the profile has not been loaded and the gaia_id has not been written yet.
  return !GetGaiaId().empty() || !GetUserName().empty();
}

void ProfileAttributesIOS::ClearIsNewProfile() {
  SetBool(storage_, kNewProfile, false);
}

ProfileAttributesIOS::SessionIds ProfileAttributesIOS::GetDiscardedSessions()
    const {
  return GetStringSet<SessionIds>(storage_, kDiscardedSessions);
}

void ProfileAttributesIOS::SetAuthenticationInfo(std::string_view gaia_id,
                                                 std::string_view user_name) {
  SetString(storage_, kGaiaIdKey, gaia_id);
  SetString(storage_, kUserNameKey, user_name);
}

void ProfileAttributesIOS::SetHasAuthenticationError(bool value) {
  SetBool(storage_, kIsAuthErrorKey, value);
}

void ProfileAttributesIOS::SetAttachedGaiaIds(const GaiaIdSet& gaia_ids) {
  SetStringSet(storage_, kAttachedGaiaIdsKey, gaia_ids);
}

void ProfileAttributesIOS::SetLastActiveTime(base::Time time) {
  SetTime(storage_, kActiveTimeKey, time);
}

void ProfileAttributesIOS::SetDiscardedSessions(const SessionIds& session_ids) {
  SetStringSet(storage_, kDiscardedSessions, session_ids);
}

base::Value::Dict ProfileAttributesIOS::GetStorage() && {
  return std::move(storage_);
}

ProfileAttributesIOS::ProfileAttributesIOS(std::string_view profile_name,
                                           base::Value::Dict storage)
    : profile_name_(profile_name), storage_(std::move(storage)) {
  DCHECK(!profile_name_.empty());
}
