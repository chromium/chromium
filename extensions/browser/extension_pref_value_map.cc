// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_pref_value_map.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_value_map.h"
#include "extensions/common/api/types.h"
#include "extensions/common/extension_id.h"

struct ExtensionPrefValueMap::ExtensionEntry {
  // Installation time of the extension.
  base::Time install_time;
  // Whether extension is enabled in the profile.
  bool enabled;
  // Whether the extension has access to the incognito profile.
  bool incognito_enabled;
  // Extension controlled preferences for the regular profile.
  PrefValueMap regular_profile_preferences;
  // Extension controlled preferences that should *only* apply to the regular
  // profile.
  PrefValueMap regular_only_profile_preferences;
  // Persistent extension controlled preferences for the incognito profile,
  // empty for regular profile ExtensionPrefStore.
  PrefValueMap incognito_profile_preferences_persistent;
  // Session only extension controlled preferences for the incognito profile.
  // These preferences are deleted when the incognito profile is destroyed.
  PrefValueMap incognito_profile_preferences_session_only;
};

ExtensionPrefValueMap::ExtensionPrefValueMap() : destroyed_(false) {
}

ExtensionPrefValueMap::~ExtensionPrefValueMap() {
  if (!destroyed_) {
    NotifyOfDestruction();
    destroyed_ = true;
  }
}

void ExtensionPrefValueMap::Shutdown() {
  NotifyOfDestruction();
  destroyed_ = true;
}

void ExtensionPrefValueMap::SetExtensionPref(const std::string& ext_id,
                                             const std::string& key,
                                             ChromeSettingScope scope,
                                             base::Value value) {
  PrefValueMap* prefs = GetExtensionPrefValueMap(ext_id, scope);
  if (prefs->SetValue(key, std::move(value)))
    NotifyPrefValueChanged(key);
}

void ExtensionPrefValueMap::RemoveExtensionPref(const std::string& ext_id,
                                                const std::string& key,
                                                ChromeSettingScope scope) {
  PrefValueMap* prefs = GetExtensionPrefValueMap(ext_id, scope);
  if (prefs->RemoveValue(key))
    NotifyPrefValueChanged(key);
}

bool ExtensionPrefValueMap::CanExtensionControlPref(
    const extensions::ExtensionId& extension_id,
    const std::string& pref_key,
    bool incognito) const {
  auto ext = entries_.find(extension_id);
  if (ext == entries_.end()) {
    NOTREACHED_IN_MIGRATION()
        << "Extension " << extension_id
        << " is not registered but accesses pref " << pref_key
        << " (incognito: " << incognito << ")." << " http://crbug.com/454513";
    return false;
  }

  if (incognito && !ext->second->incognito_enabled)
    return false;

  auto winner = GetEffectivePrefValueController(pref_key, incognito, nullptr);
  if (winner == entries_.end())
    return true;

  return winner->second->install_time <= ext->second->install_time;
}

void ExtensionPrefValueMap::ClearAllIncognitoSessionOnlyPreferences() {
  typedef std::set<std::string> KeySet;
  KeySet deleted_keys;

  for (const auto& entry : entries_) {
    PrefValueMap& inc_prefs =
        entry.second->incognito_profile_preferences_session_only;
    for (const auto& pref : inc_prefs)
      deleted_keys.insert(pref.first);
    inc_prefs.Clear();
  }

  for (const auto& key : deleted_keys)
    NotifyPrefValueChanged(key);
}

bool ExtensionPrefValueMap::DoesExtensionControlPref(
    const extensions::ExtensionId& extension_id,
    const std::string& pref_key,
    bool* from_incognito) const {
  bool incognito = (from_incognito != nullptr);
  auto winner =
      GetEffectivePrefValueController(pref_key, incognito, from_incognito);
  if (winner == entries_.end())
    return false;
  return winner->first == extension_id;
}

void ExtensionPrefValueMap::RegisterExtension(const std::string& ext_id,
                                              const base::Time& install_time,
                                              bool is_enabled,
                                              bool is_incognito_enabled) {
  auto& entry = entries_[ext_id];
  if (!entry) {
    entry = std::make_unique<ExtensionEntry>();
    entry->install_time = install_time;
  }

  entry->enabled = is_enabled;
  entry->incognito_enabled = is_incognito_enabled;
}

void ExtensionPrefValueMap::UnregisterExtension(const std::string& ext_id) {
  auto i = entries_.find(ext_id);
  if (i == entries_.end())
    return;
  std::set<std::string> keys;  // keys set by this extension
  GetExtensionControlledKeys(*(i->second.get()), &keys);

  entries_.erase(i);

  NotifyPrefValueChanged(keys);
}

void ExtensionPrefValueMap::SetExtensionState(const std::string& ext_id,
                                              bool is_enabled) {
  ExtensionEntryMap::const_iterator i = entries_.find(ext_id);
  // This may happen when sync sets the extension state for an
  // extension that is not installed.
  if (i == entries_.end())
    return;
  if (i->second->enabled == is_enabled)
    return;
  std::set<std::string> keys;  // keys set by this extension
  GetExtensionControlledKeys(*(i->second), &keys);
  i->second->enabled = is_enabled;
  NotifyPrefValueChanged(keys);
}

void ExtensionPrefValueMap::SetExtensionIncognitoState(
    const std::string& ext_id,
    bool is_incognito_enabled) {
  ExtensionEntryMap::const_iterator i = entries_.find(ext_id);
  // This may happen when sync sets the extension state for an
  // extension that is not installed.
  if (i == entries_.end())
    return;
  if (i->second->incognito_enabled == is_incognito_enabled)
    return;
  std::set<std::string> keys;  // keys set by this extension
  GetExtensionControlledKeys(*(i->second), &keys);
  i->second->incognito_enabled = is_incognito_enabled;
  NotifyPrefValueChanged(keys);
}

PrefValueMap* ExtensionPrefValueMap::GetExtensionPrefValueMap(
    const std::string& ext_id,
    ChromeSettingScope scope) {
  ExtensionEntryMap::const_iterator i = entries_.find(ext_id);
  CHECK(i != entries_.end());
  switch (scope) {
    case ChromeSettingScope::kRegular:
      return &i->second->regular_profile_preferences;
    case ChromeSettingScope::kRegularOnly:
      return &i->second->regular_only_profile_preferences;
    case ChromeSettingScope::kIncognitoPersistent:
      return &i->second->incognito_profile_preferences_persistent;
    case ChromeSettingScope::kIncognitoSessionOnly:
      return &i->second->incognito_profile_preferences_session_only;
    case ChromeSettingScope::kNone:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

const PrefValueMap* ExtensionPrefValueMap::GetExtensionPrefValueMap(
    const std::string& ext_id,
    ChromeSettingScope scope) const {
  auto i = entries_.find(ext_id);
  CHECK(i != entries_.end());
  switch (scope) {
    case ChromeSettingScope::kRegular:
      return &i->second->regular_profile_preferences;
    case ChromeSettingScope::kRegularOnly:
      return &i->second->regular_only_profile_preferences;
    case ChromeSettingScope::kIncognitoPersistent:
      return &i->second->incognito_profile_preferences_persistent;
    case ChromeSettingScope::kIncognitoSessionOnly:
      return &i->second->incognito_profile_preferences_session_only;
    case ChromeSettingScope::kNone:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void ExtensionPrefValueMap::GetExtensionControlledKeys(
    const ExtensionEntry& entry,
    std::set<std::string>* out) const {
  PrefValueMap::const_iterator i;

  const PrefValueMap& regular_prefs = entry.regular_profile_preferences;
  for (i = regular_prefs.begin(); i != regular_prefs.end(); ++i)
    out->insert(i->first);

  const PrefValueMap& regular_only_prefs =
      entry.regular_only_profile_preferences;
  for (i = regular_only_prefs.begin(); i != regular_only_prefs.end(); ++i)
    out->insert(i->first);

  const PrefValueMap& inc_prefs_pers =
      entry.incognito_profile_preferences_persistent;
  for (i = inc_prefs_pers.begin(); i != inc_prefs_pers.end(); ++i)
    out->insert(i->first);

  const PrefValueMap& inc_prefs_session =
      entry.incognito_profile_preferences_session_only;
  for (i = inc_prefs_session.begin(); i != inc_prefs_session.end(); ++i)
    out->insert(i->first);
}

const base::Value* ExtensionPrefValueMap::GetEffectivePrefValue(
    const std::string& key,
    bool incognito,
    bool* from_incognito) const {
  auto winner = GetEffectivePrefValueController(key, incognito, from_incognito);
  if (winner == entries_.end())
    return nullptr;

  const base::Value* value = nullptr;
  const std::string& ext_id = winner->first;

  // First search for incognito session only preferences.
  if (incognito) {
    DCHECK(winner->second->incognito_enabled);
    const PrefValueMap* prefs = GetExtensionPrefValueMap(
        ext_id, ChromeSettingScope::kIncognitoSessionOnly);
    prefs->GetValue(key, &value);
    if (value)
      return value;

    // If no incognito session only preference exists, fall back to persistent
    // incognito preference.
    prefs = GetExtensionPrefValueMap(ext_id,
                                     ChromeSettingScope::kIncognitoPersistent);
    prefs->GetValue(key, &value);
    if (value)
      return value;
  } else {
    // Regular-only preference.
    const PrefValueMap* prefs =
        GetExtensionPrefValueMap(ext_id, ChromeSettingScope::kRegularOnly);
    prefs->GetValue(key, &value);
    if (value)
      return value;
  }

  // Regular preference.
  const PrefValueMap* prefs =
      GetExtensionPrefValueMap(ext_id, ChromeSettingScope::kRegular);
  prefs->GetValue(key, &value);
  return value;
}

ExtensionPrefValueMap::ExtensionEntryMap::const_iterator
ExtensionPrefValueMap::GetEffectivePrefValueController(
    const std::string& key,
    bool incognito,
    bool* from_incognito) const {
  auto winner = entries_.cend();
  base::Time winners_install_time;

  for (auto i = entries_.cbegin(); i != entries_.cend(); ++i) {
    const std::string& ext_id = i->first;
    const base::Time& install_time = i->second->install_time;
    const bool enabled = i->second->enabled;
    const bool incognito_enabled = i->second->incognito_enabled;

    if (!enabled)
      continue;
    if (install_time < winners_install_time)
      continue;
    if (incognito && !incognito_enabled)
      continue;

    const base::Value* value = nullptr;
    const PrefValueMap* prefs =
        GetExtensionPrefValueMap(ext_id, ChromeSettingScope::kRegular);
    if (prefs->GetValue(key, &value)) {
      winner = i;
      winners_install_time = install_time;
      if (from_incognito)
        *from_incognito = false;
    }

    if (!incognito) {
      prefs =
          GetExtensionPrefValueMap(ext_id, ChromeSettingScope::kRegularOnly);
      if (prefs->GetValue(key, &value)) {
        winner = i;
        winners_install_time = install_time;
        if (from_incognito)
          *from_incognito = false;
      }
      // Ignore the following prefs, because they're incognito-only.
      continue;
    }

    prefs = GetExtensionPrefValueMap(ext_id,
                                     ChromeSettingScope::kIncognitoPersistent);
    if (prefs->GetValue(key, &value)) {
      winner = i;
      winners_install_time = install_time;
      if (from_incognito)
        *from_incognito = true;
    }

    prefs = GetExtensionPrefValueMap(ext_id,
                                     ChromeSettingScope::kIncognitoSessionOnly);
    if (prefs->GetValue(key, &value)) {
      winner = i;
      winners_install_time = install_time;
      if (from_incognito)
        *from_incognito = true;
    }
  }
  return winner;
}

void ExtensionPrefValueMap::AddObserver(
    ExtensionPrefValueMap::Observer* observer) {
  observers_.AddObserver(observer);

  // Collect all currently used keys and notify the new observer.
  std::set<std::string> keys;
  ExtensionEntryMap::const_iterator i;
  for (i = entries_.begin(); i != entries_.end(); ++i)
    GetExtensionControlledKeys(*(i->second), &keys);

  std::set<std::string>::const_iterator j;
  for (j = keys.begin(); j != keys.end(); ++j)
    observer->OnPrefValueChanged(*j);
}

void ExtensionPrefValueMap::RemoveObserver(
    ExtensionPrefValueMap::Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::string ExtensionPrefValueMap::GetExtensionControllingPref(
    const std::string& pref_key) const {
  auto winner = GetEffectivePrefValueController(pref_key, false, nullptr);
  if (winner == entries_.end())
    return std::string();
  return winner->first;
}

void ExtensionPrefValueMap::NotifyInitializationCompleted() {
  for (auto& observer : observers_)
    observer.OnInitializationCompleted();
}

void ExtensionPrefValueMap::NotifyPrefValueChanged(
    const std::set<std::string>& keys) {
  for (const auto& key : keys)
    NotifyPrefValueChanged(key);
}

void ExtensionPrefValueMap::NotifyPrefValueChanged(const std::string& key) {
  for (auto& observer : observers_)
    observer.OnPrefValueChanged(key);
}

void ExtensionPrefValueMap::NotifyOfDestruction() {
  for (auto& observer : observers_)
    observer.OnExtensionPrefValueMapDestruction();
}
