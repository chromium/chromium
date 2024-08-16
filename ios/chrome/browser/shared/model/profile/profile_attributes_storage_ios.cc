// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/i18n/case_conversion.h"
#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace {
const char kActiveTimeKey[] = "active_time";
const char kGAIAIdKey[] = "gaia_id";
const char kIsAuthErrorKey[] = "is_auth_error";
const char kUserNameKey[] = "user_name";
}  // namespace

ProfileAttributesStorageIOS::ProfileAttributesStorageIOS(PrefService* prefs)
    : prefs_(prefs) {
  // Populate the cache
  for (const auto pair : prefs_->GetDict(prefs::kBrowserStateInfoCache)) {
    sorted_keys_.push_back(pair.first);
  }
  base::ranges::sort(sorted_keys_);
}

ProfileAttributesStorageIOS::~ProfileAttributesStorageIOS() = default;

void ProfileAttributesStorageIOS::AddBrowserState(std::string_view name,
                                                  std::string_view gaia_id,
                                                  std::string_view user_name) {
  CHECK_EQ(GetIndexOfBrowserStateWithName(name), std::string::npos);
  ScopedDictPrefUpdate update(prefs_, prefs::kBrowserStateInfoCache);
  base::Value::Dict& cache = update.Get();

  const int browser_states_count =
      prefs_->GetInteger(prefs::kBrowserStatesNumCreated);
  prefs_->SetInteger(prefs::kBrowserStatesNumCreated, browser_states_count + 1);

  base::Value::List last_active_browser_states =
      prefs_->GetList(prefs::kBrowserStatesLastActive).Clone();
  last_active_browser_states.Append(base::Value(name));
  prefs_->SetList(prefs::kBrowserStatesLastActive,
                  std::move(last_active_browser_states));

  base::Value::Dict info;
  info.Set(kGAIAIdKey, gaia_id);
  info.Set(kUserNameKey, user_name);
  cache.Set(name, std::move(info));
  sorted_keys_.insert(base::ranges::upper_bound(sorted_keys_, name),
                      std::string(name));
}

void ProfileAttributesStorageIOS::RemoveBrowserState(std::string_view name) {
  CHECK_NE(GetIndexOfBrowserStateWithName(name), std::string::npos);
  ScopedDictPrefUpdate update(prefs_, prefs::kBrowserStateInfoCache);
  base::Value::Dict& cache = update.Get();

  const int browser_states_count =
      prefs_->GetInteger(prefs::kBrowserStatesNumCreated);
  DCHECK_GE(browser_states_count, 1);
  prefs_->SetInteger(prefs::kBrowserStatesNumCreated, browser_states_count - 1);

  base::Value::List last_active_browser_states =
      prefs_->GetList(prefs::kBrowserStatesLastActive).Clone();
  last_active_browser_states.EraseValue(base::Value(name));
  prefs_->SetList(prefs::kBrowserStatesLastActive,
                  std::move(last_active_browser_states));

  cache.Remove(name);
  sorted_keys_.erase(base::ranges::find(sorted_keys_, name));
}

size_t ProfileAttributesStorageIOS::GetNumberOfBrowserStates() const {
  return sorted_keys_.size();
}

size_t ProfileAttributesStorageIOS::GetIndexOfBrowserStateWithName(
    std::string_view name) const {
  auto iterator = base::ranges::lower_bound(sorted_keys_, name);
  if (iterator == sorted_keys_.end() || *iterator != name) {
    return std::string::npos;
  }
  return std::distance(sorted_keys_.begin(), iterator);
}

const std::string& ProfileAttributesStorageIOS::GetNameOfBrowserStateAtIndex(
    size_t index) const {
  return sorted_keys_[index];
}

const std::string& ProfileAttributesStorageIOS::GetGAIAIdOfBrowserStateAtIndex(
    size_t index) const {
  const base::Value::Dict* value = GetInfoForBrowserStateAtIndex(index);
  const std::string* gaia_id = value->FindString(kGAIAIdKey);
  return gaia_id ? *gaia_id : base::EmptyString();
}

const std::string&
ProfileAttributesStorageIOS::GetUserNameOfBrowserStateAtIndex(
    size_t index) const {
  const base::Value::Dict* value = GetInfoForBrowserStateAtIndex(index);
  const std::string* user_name = value->FindString(kUserNameKey);
  return user_name ? *user_name : base::EmptyString();
}

bool ProfileAttributesStorageIOS::BrowserStateIsAuthenticatedAtIndex(
    size_t index) const {
  // The browser state is authenticated if the gaia_id of the info is not empty.
  // If it is empty, also check if the user name is not empty.  This latter
  // check is needed in case the browser state has not been loaded yet and the
  // gaia_id property has not yet been written.
  return !GetGAIAIdOfBrowserStateAtIndex(index).empty() ||
         !GetUserNameOfBrowserStateAtIndex(index).empty();
}

bool ProfileAttributesStorageIOS::BrowserStateIsAuthErrorAtIndex(
    size_t index) const {
  return GetInfoForBrowserStateAtIndex(index)
      ->FindBool(kIsAuthErrorKey)
      .value_or(false);
}

void ProfileAttributesStorageIOS::SetAuthInfoOfBrowserStateAtIndex(
    size_t index,
    std::string_view gaia_id,
    std::string_view user_name) {
  // If both gaia_id and username are unchanged, abort early.
  if (gaia_id == GetGAIAIdOfBrowserStateAtIndex(index) &&
      user_name == GetUserNameOfBrowserStateAtIndex(index)) {
    return;
  }

  base::Value::Dict info = GetInfoForBrowserStateAtIndex(index)->Clone();
  info.Set(kGAIAIdKey, base::Value(gaia_id));
  info.Set(kUserNameKey, base::Value(user_name));
  SetInfoForBrowserStateAtIndex(index, std::move(info));
}

void ProfileAttributesStorageIOS::SetBrowserStateIsAuthErrorAtIndex(
    size_t index,
    bool value) {
  if (value == BrowserStateIsAuthErrorAtIndex(index)) {
    return;
  }

  base::Value::Dict info = GetInfoForBrowserStateAtIndex(index)->Clone();
  info.Set(kIsAuthErrorKey, base::Value(value));
  SetInfoForBrowserStateAtIndex(index, std::move(info));
}

base::Time ProfileAttributesStorageIOS::GetLastActiveTimeOfBrowserStateAtIndex(
    size_t index) const {
  std::optional<base::Time> last_active_time = base::ValueToTime(
      GetInfoForBrowserStateAtIndex(index)->Find(kActiveTimeKey));
  return last_active_time.value_or(base::Time());
}

void ProfileAttributesStorageIOS::SetLastActiveTimeOfBrowserStateAtIndex(
    size_t index,
    base::Time time) {
  base::Value::Dict info = GetInfoForBrowserStateAtIndex(index)->Clone();
  info.Set(kActiveTimeKey, base::TimeToValue(time));
  SetInfoForBrowserStateAtIndex(index, std::move(info));
}

void ProfileAttributesStorageIOS::SetBrowserStateForSceneID(
    std::string_view scene_id,
    std::string_view browser_state_name) {
  DCHECK(!browser_state_name.empty());
  ScopedDictPrefUpdate update(prefs_, prefs::kBrowserStateForScene);
  base::Value::Dict& cache = update.Get();
  cache.Set(scene_id, browser_state_name);
}

void ProfileAttributesStorageIOS::ClearBrowserStateForSceneID(
    std::string_view scene_id) {
  ScopedDictPrefUpdate update(prefs_, prefs::kBrowserStateForScene);
  base::Value::Dict& cache = update.Get();
  cache.Remove(scene_id);
}

const std::string& ProfileAttributesStorageIOS::GetBrowserStateNameForSceneID(
    std::string_view scene_id) {
  const std::string* browser_state_name =
      prefs_->GetDict(prefs::kBrowserStateForScene).FindString(scene_id);
  return browser_state_name ? *browser_state_name : base::EmptyString();
}

// static
void ProfileAttributesStorageIOS::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kBrowserStateInfoCache);
  registry->RegisterIntegerPref(prefs::kBrowserStatesNumCreated, 0);
  registry->RegisterListPref(prefs::kBrowserStatesLastActive);
  registry->RegisterDictionaryPref(prefs::kBrowserStateForScene);
}

const base::Value::Dict*
ProfileAttributesStorageIOS::GetInfoForBrowserStateAtIndex(size_t index) const {
  DCHECK_LT(index, GetNumberOfBrowserStates());
  return prefs_->GetDict(prefs::kBrowserStateInfoCache)
      .FindDict(sorted_keys_[index]);
}

void ProfileAttributesStorageIOS::SetInfoForBrowserStateAtIndex(
    size_t index,
    base::Value::Dict info) {
  ScopedDictPrefUpdate update(prefs_, prefs::kBrowserStateInfoCache);
  update->Set(sorted_keys_[index], std::move(info));
}
