// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_prefs_helper.h"

#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_helper_factory.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/api/types.h"
#include "extensions/common/extension_id.h"

namespace extensions {

using content::BrowserContext;

// static
ExtensionPrefsHelper* ExtensionPrefsHelper::Get(BrowserContext* context) {
  return ExtensionPrefsHelperFactory::GetForBrowserContext(context);
}

ExtensionPrefsHelper::ExtensionPrefsHelper(ExtensionPrefs* prefs,
                                           ExtensionPrefValueMap* value_map)
    : prefs_(prefs), value_map_(value_map) {}

ExtensionPrefsHelper::~ExtensionPrefsHelper() = default;

void ExtensionPrefsHelper::SetExtensionControlledPref(
    const ExtensionId& extension_id,
    const std::string& pref_key,
    ChromeSettingScope scope,
    base::Value value) {
#ifndef NDEBUG
  const PrefService::Preference* pref =
      prefs_->pref_service()->FindPreference(pref_key);
  DCHECK(pref) << "Extension controlled preference key " << pref_key
               << " not registered.";
  DCHECK_EQ(pref->GetType(), value.type())
      << "Extension controlled preference " << pref_key << " has wrong type.";
#endif

  std::string scope_string;
  // ScopeToPrefName() returns false if the scope is not persisted.
  if (pref_names::ScopeToPrefName(scope, &scope_string)) {
    // Also store in persisted Preferences file to recover after a
    // browser restart.
    ExtensionPrefs::ScopedDictionaryUpdate update(prefs_, extension_id,
                                                  scope_string);
    auto preference = update.Create();
    preference->SetWithoutPathExpansion(pref_key, value.Clone());
  }
  value_map_->SetExtensionPref(extension_id, pref_key, scope, std::move(value));
}

void ExtensionPrefsHelper::RemoveExtensionControlledPref(
    const ExtensionId& extension_id,
    const std::string& pref_key,
    ChromeSettingScope scope) {
  DCHECK(prefs_->pref_service()->FindPreference(pref_key))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  std::string scope_string;
  if (pref_names::ScopeToPrefName(scope, &scope_string)) {
    ExtensionPrefs::ScopedDictionaryUpdate update(prefs_, extension_id,
                                                  scope_string);
    auto preference = update.Get();
    if (preference) {
      preference->RemoveWithoutPathExpansion(pref_key, nullptr);
    }
  }
  value_map_->RemoveExtensionPref(extension_id, pref_key, scope);
}

bool ExtensionPrefsHelper::CanExtensionControlPref(
    const ExtensionId& extension_id,
    const std::string& pref_key,
    bool incognito) {
  DCHECK(prefs_->pref_service()->FindPreference(pref_key))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  return value_map_->CanExtensionControlPref(extension_id, pref_key, incognito);
}

bool ExtensionPrefsHelper::DoesExtensionControlPref(
    const ExtensionId& extension_id,
    const std::string& pref_key,
    bool* from_incognito) {
  DCHECK(prefs_->pref_service()->FindPreference(pref_key))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  return value_map_->DoesExtensionControlPref(extension_id, pref_key,
                                              from_incognito);
}

}  // namespace extensions
