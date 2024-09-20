// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"

#include "base/check.h"
#include "base/json/values_util.h"
#include "base/strings/string_util.h"

namespace {
const char kActiveTimeKey[] = "active_time";
const char kGaiaIdKey[] = "gaia_id";
const char kIsAuthErrorKey[] = "is_auth_error";
const char kAttachedGaiaIdsKey[] = "attached_gaia_ids";
const char kUserNameKey[] = "user_name";
}  // namespace

ProfileAttributesIOS::ProfileAttributesIOS(std::string_view profile_name,
                                           const base::Value::Dict* attrs)
    : profile_name_(profile_name),
      storage_(attrs ? attrs->Clone() : base::Value::Dict()) {
  DCHECK(!profile_name_.empty());
}

ProfileAttributesIOS::ProfileAttributesIOS(ProfileAttributesIOS&&) = default;

ProfileAttributesIOS& ProfileAttributesIOS::operator=(ProfileAttributesIOS&&) =
    default;

ProfileAttributesIOS::~ProfileAttributesIOS() = default;

const std::string& ProfileAttributesIOS::ProfileAttributesIOS::GetProfileName()
    const {
  return profile_name_;
}

const std::string& ProfileAttributesIOS::GetGaiaId() const {
  if (const std::string* gaia_id = storage_.FindString(kGaiaIdKey)) {
    return *gaia_id;
  }
  return base::EmptyString();
}

const std::string& ProfileAttributesIOS::GetUserName() const {
  if (const std::string* user_name = storage_.FindString(kUserNameKey)) {
    return *user_name;
  }
  return base::EmptyString();
}

bool ProfileAttributesIOS::HasAuthenticationError() const {
  return storage_.FindBool(kIsAuthErrorKey).value_or(false);
}

ProfileAttributesIOS::GaiaIdSet ProfileAttributesIOS::GetAttachedGaiaIds()
    const {
  GaiaIdSet gaia_id_set;
  if (const base::Value::List* gaia_ids =
          storage_.FindList(kAttachedGaiaIdsKey)) {
    for (const auto& gaia_id_value : *gaia_ids) {
      if (!gaia_id_value.is_string()) {
        continue;
      }
      gaia_id_set.insert(gaia_id_value.GetString());
    }
  }
  return gaia_id_set;
}

base::Time ProfileAttributesIOS::GetLastActiveTime() const {
  return base::ValueToTime(storage_.Find(kActiveTimeKey))
      .value_or(base::Time());
}

bool ProfileAttributesIOS::IsAuthenticated() const {
  // The profile is authenticated if the gaia_id is not empty. If it is empty,
  // check if the username is not empty. This latter check is needed in case
  // the profile has not been loaded and the gaia_id has not been written yet.
  return !GetGaiaId().empty() || !GetUserName().empty();
}

void ProfileAttributesIOS::SetAuthenticationInfo(std::string_view gaia_id,
                                                 std::string_view user_name) {
  storage_.Set(kGaiaIdKey, gaia_id);
  storage_.Set(kUserNameKey, user_name);
}

void ProfileAttributesIOS::SetHasAuthenticationError(bool value) {
  storage_.Set(kIsAuthErrorKey, value);
}

void ProfileAttributesIOS::SetAttachedGaiaIds(const GaiaIdSet& gaia_ids) {
  base::Value::List gaia_id_list;
  for (auto const& iterator : gaia_ids) {
    gaia_id_list.Append(iterator);
  }
  storage_.Set(kAttachedGaiaIdsKey, std::move(gaia_id_list));
}

void ProfileAttributesIOS::SetLastActiveTime(base::Time time) {
  storage_.Set(kActiveTimeKey, base::TimeToValue(time));
}

base::Value::Dict ProfileAttributesIOS::GetStorage() && {
  return std::move(storage_);
}
