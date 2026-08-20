// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"

#include "base/check.h"
#include "base/json/values_util.h"
#include "base/strings/string_util.h"
#include "google_apis/gaia/gaia_id.h"

namespace {

using Dict = base::DictValue;
using Path = std::initializer_list<std::string_view>;

// Constants used as key in the dictionary.
constexpr std::string_view kActiveTimeKey = "active_time";
constexpr std::string_view kGaiaIdKey = "gaia_id";
constexpr std::string_view kIsAuthErrorKey = "is_auth_error";
constexpr std::string_view kAttachedGaiaIdsKey = "attached_gaia_ids";
constexpr std::string_view kUserNameKey = "user_name";
constexpr std::string_view kNewProfile = "new_profile";
constexpr std::string_view kIsFullyInitializedKey = "fully_initialized";
constexpr std::string_view kIsDeletedProfile = "deleted_profile";
constexpr std::string_view kDiscardedSessions = "discarded_sessions";
constexpr std::string_view kSessionScopedPrefs = "session_scoped_prefs";
constexpr std::string_view kNotificationPermissions =
    "notification_permissions";

// Helper class to convert a V into a base::Value using partial specialisation.
template <typename T>
struct From {};

// Partial specialisation of `From<>` for `std::nullopt_t`.
template <>
struct From<std::nullopt_t> {
  static base::Value operator()(std::nullopt_t value) { return base::Value(); }
};

// Partial specialisation of `From<>` for `bool`.
template <>
struct From<bool> {
  static base::Value operator()(bool value) {
    return value ? base::Value(true) : base::Value();
  }
};

// Partial specialisation of `From<>` for `base::Time`.
template <>
struct From<base::Time> {
  static base::Value operator()(base::Time value) {
    return value != base::Time() ? base::TimeToValue(value) : base::Value();
  }
};

// Partial specialisation of `From<>` for `std::string`.
template <>
struct From<std::string> {
  static base::Value operator()(const std::string& value) {
    return !value.empty() ? base::Value(value) : base::Value();
  }
};

// Partial specialisation of `From<>` for `std::string_view`.
template <>
struct From<std::string_view> {
  static base::Value operator()(std::string_view value) {
    return !value.empty() ? base::Value(value) : base::Value();
  }
};

// Partial specialisation of `From<>` for `GaiaId`.
template <>
struct From<GaiaId> {
  static base::Value operator()(const GaiaId& value) {
    return !value.empty() ? base::Value(value.ToString()) : base::Value();
  }
};

// Partial specialisation of `From<>` for `base::DictValue`.
template <>
struct From<base::DictValue> {
  static base::Value operator()(base::DictValue value) {
    return !value.empty() ? base::Value(std::move(value)) : base::Value();
  }
};

// Partial specialisation of `From<>` for `base::ListValue`.
template <>
struct From<base::ListValue> {
  static base::Value operator()(base::ListValue value) {
    return !value.empty() ? base::Value(std::move(value)) : base::Value();
  }
};

// Partial specialisation of `From<>` for `std::set<V, Less>`.
template <typename V, typename Less>
struct From<std::set<V, Less>> {
  static base::Value operator()(const std::set<V, Less>& value) {
    if (value.empty()) {
      return base::Value();
    }
    base::ListValue list;
    for (const auto& item : value) {
      list.Append(From<V>{}(item));
    }
    return base::Value(std::move(list));
  }
};

// Helper class to convert a base::Value* to V using partial specialisation.
template <typename V>
struct Into {};

// Partial specialisation of `Into<>` for `bool`.
template <>
struct Into<bool> {
  using output = bool;
  static output operator()(const base::Value* value) {
    return value ? value->GetIfBool().value_or(false) : false;
  }
};

// Partial specialisation of `Into<>` for `base::Time`.
template <>
struct Into<base::Time> {
  using output = base::Time;
  static output operator()(const base::Value* value) {
    return base::ValueToTime(value).value_or(base::Time());
  }
};

// Partial specialisation of `Into<>` for `std::string`.
template <>
struct Into<std::string> {
  using output = const std::string&;
  static output operator()(const base::Value* value) {
    if (value) {
      if (const std::string* string = value->GetIfString()) {
        return *string;
      }
    }

    return base::EmptyString();
  }
};

// Partial specialisation of `Into<>` for `GaiaId`.
template <>
struct Into<GaiaId> {
  using output = GaiaId;
  static output operator()(const base::Value* value) {
    return GaiaId(Into<std::string>{}(value));
  }
};

// Partial specialisation of `Into<>` for `base::DictValue`.
template <>
struct Into<base::DictValue> {
  using output = const base::DictValue*;
  static output operator()(const base::Value* value) {
    return value ? value->GetIfDict() : nullptr;
  }
};

// Partial specialisation of `Into<>` for `base::ListValue`.
template <>
struct Into<base::ListValue> {
  using output = const base::ListValue*;
  static output operator()(const base::Value* value) {
    return value ? value->GetIfList() : nullptr;
  }
};

// Partial specialisation of `Into<>` for `std::set<V, Less>`.
template <typename V, typename Less>
struct Into<std::set<V, Less>> {
  using output = std::set<V, Less>;
  static output operator()(const base::Value* value) {
    std::set<V, Less> set;
    if (const auto* list = Into<base::ListValue>{}(value)) {
      for (const base::Value& item : *list) {
        set.emplace(Into<V>{}(&item));
      }
    }
    return set;
  }
};

// Retrieves a value from the dictionary.
const base::Value* GetValue(const base::DictValue& dict, Path path) {
  size_t index = 0;
  const size_t count = path.size();
  const base::DictValue* current_dict = &dict;
  for (std::string_view key : path) {
    if (++index == count) {
      return current_dict->Find(key);
    }

    current_dict = current_dict->FindDict(key);
    if (!current_dict) {
      break;
    }
  }
  return nullptr;
}

// Stores `value` in the dictionary or erase the key if value is `NONE`.
void SetValue(base::DictValue& dict, Path path, base::Value value) {
  size_t index = 0;
  const size_t count = path.size();
  base::DictValue* current_dict = &dict;
  for (std::string_view key : path) {
    if (++index == count) {
      if (value.type() == base::Value::Type::NONE) {
        current_dict->Remove(key);
      } else {
        current_dict->Set(key, std::move(value));
      }
      return;
    }

    base::Value* current_value = current_dict->Find(key);
    if (current_value) {
      current_dict = current_value->GetIfDict();
      if (!current_dict) {
        return;
      }
    } else {
      current_value = current_dict->Set(key, base::Value(base::DictValue()));
      current_dict = &(current_value->GetDict());
    }
  }
}

// Retrieves a value from the dictionary (as `Into<V>::output`).
template <typename V>
typename Into<V>::output Get(const base::DictValue& dict, Path path) {
  return Into<V>{}(GetValue(dict, std::move(path)));
}

// Stores `value` into the dictionary.
template <typename V>
void Set(base::DictValue& dict, Path path, V&& value) {
  return SetValue(dict, std::move(path),
                  From<std::decay_t<V>>{}(std::forward<V>(value)));
}

}  // namespace

// static
ProfileAttributesIOS ProfileAttributesIOS::CreateNew(
    std::string_view profile_name) {
  base::DictValue dict;
  Set(dict, {kNewProfile}, true);
  return ProfileAttributesIOS(profile_name, std::move(dict));
}

// static
ProfileAttributesIOS ProfileAttributesIOS::WithAttrs(
    std::string_view profile_name,
    const base::DictValue& storage) {
  return ProfileAttributesIOS(profile_name, storage.Clone());
}

ProfileAttributesIOS ProfileAttributesIOS::DeletedProfile(
    std::string_view profile_name) {
  base::DictValue dict;
  Set(dict, {kIsDeletedProfile}, true);
  return ProfileAttributesIOS(profile_name, std::move(dict));
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
  return Get<bool>(storage_, {kNewProfile});
}

bool ProfileAttributesIOS::IsFullyInitialized() const {
  return Get<bool>(storage_, {kIsFullyInitializedKey});
}

bool ProfileAttributesIOS::IsDeletedProfile() const {
  return Get<bool>(storage_, {kIsDeletedProfile});
}

GaiaId ProfileAttributesIOS::GetGaiaId() const {
  return Get<GaiaId>(storage_, {kGaiaIdKey});
}

const std::string& ProfileAttributesIOS::GetUserName() const {
  return Get<std::string>(storage_, {kUserNameKey});
}

bool ProfileAttributesIOS::HasAuthenticationError() const {
  return Get<bool>(storage_, {kIsAuthErrorKey});
}

ProfileAttributesIOS::GaiaIdSet ProfileAttributesIOS::GetAttachedGaiaIds()
    const {
  return Get<GaiaIdSet>(storage_, {kAttachedGaiaIdsKey});
}

base::Time ProfileAttributesIOS::GetLastActiveTime() const {
  return Get<base::Time>(storage_, {kActiveTimeKey});
}

bool ProfileAttributesIOS::IsAuthenticated() const {
  // The profile is authenticated if the gaia_id is not empty. If it is empty,
  // check if the username is not empty. This latter check is needed in case
  // the profile has not been loaded and the gaia_id has not been written yet.
  return !GetGaiaId().empty() || !GetUserName().empty();
}

ProfileAttributesIOS::SessionIds ProfileAttributesIOS::GetDiscardedSessions()
    const {
  return Get<SessionIds>(storage_, {kDiscardedSessions});
}

const Dict* ProfileAttributesIOS::GetNotificationPermissions() const {
  return Get<Dict>(storage_, {kNotificationPermissions});
}

std::set<std::string> ProfileAttributesIOS::GetKnownSessions() const {
  std::set<std::string> sessions;
  if (const Dict* dict = Get<Dict>(storage_, {kSessionScopedPrefs})) {
    for (const auto [key, _] : *dict) {
      sessions.insert(key);
    }
  }
  return sessions;
}

bool ProfileAttributesIOS::HasSessionScopedPrefs(
    std::string_view session_id) const {
  return Get<Dict>(storage_, {kSessionScopedPrefs, session_id}) != nullptr;
}

bool ProfileAttributesIOS::GetSessionScopedBoolPref(
    std::string_view session_id,
    std::string_view pref_name) const {
  return Get<bool>(storage_, {kSessionScopedPrefs, session_id, pref_name});
}

base::Time ProfileAttributesIOS::GetSessionScopedTimePref(
    std::string_view session_id,
    std::string_view pref_name) const {
  using Time = base::Time;
  return Get<Time>(storage_, {kSessionScopedPrefs, session_id, pref_name});
}

void ProfileAttributesIOS::ClearIsNewProfile() {
  Set(storage_, {kNewProfile}, false);
}

void ProfileAttributesIOS::SetFullyInitialized() {
  Set(storage_, {kIsFullyInitializedKey}, true);
}

void ProfileAttributesIOS::SetAuthenticationInfo(const GaiaId& gaia_id,
                                                 std::string_view user_name) {
  Set(storage_, {kGaiaIdKey}, gaia_id);
  Set(storage_, {kUserNameKey}, user_name);
}

void ProfileAttributesIOS::SetHasAuthenticationError(bool value) {
  Set(storage_, {kIsAuthErrorKey}, value);
}

void ProfileAttributesIOS::SetAttachedGaiaIds(const GaiaIdSet& gaia_ids) {
  Set(storage_, {kAttachedGaiaIdsKey}, gaia_ids);
}

void ProfileAttributesIOS::SetLastActiveTime(base::Time time) {
  Set(storage_, {kActiveTimeKey}, time);
}

void ProfileAttributesIOS::SetDiscardedSessions(const SessionIds& session_ids) {
  Set(storage_, {kDiscardedSessions}, session_ids);
}

void ProfileAttributesIOS::SetNotificationPermissions(Dict permissions) {
  Set(storage_, {kNotificationPermissions}, std::move(permissions));
}

void ProfileAttributesIOS::ClearSessionScopedPrefs(
    std::string_view session_id) {
  Set(storage_, {kSessionScopedPrefs, session_id}, std::nullopt);
}

void ProfileAttributesIOS::SetSessionScopedBoolPref(std::string_view session_id,
                                                    std::string_view pref_name,
                                                    bool value) {
  Set(storage_, {kSessionScopedPrefs, session_id, pref_name}, value);
}

void ProfileAttributesIOS::SetSessionScopedTimePref(std::string_view session_id,
                                                    std::string_view pref_name,
                                                    base::Time value) {
  Set(storage_, {kSessionScopedPrefs, session_id, pref_name}, value);
}

base::DictValue ProfileAttributesIOS::GetStorage() && {
  return std::move(storage_);
}

ProfileAttributesIOS::ProfileAttributesIOS(std::string_view profile_name,
                                           base::DictValue storage)
    : profile_name_(profile_name), storage_(std::move(storage)) {
  DCHECK(!profile_name_.empty());
}
