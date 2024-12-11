// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/install_prefs_helper.h"

#include "base/time/time.h"
#include "extensions/browser/extension_pref_names.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions {

namespace {

// An installation parameter bundled with an extension.
constexpr PrefMap kInstallParam = {kPrefInstallParameter, PrefType::kString,
                                   PrefScope::kExtensionSpecific};

// A preference that indicates whether the extension was installed from
// the web store.
constexpr PrefMap kFromWebStore = {kPrefFromWebStore, PrefType::kBool,
                                   PrefScope::kExtensionSpecific};

// A preference that indicates whether the extension was installed as a
// default app.
constexpr PrefMap kWasInstalledByDefault = {
    kPrefWasInstalledByDefault, PrefType::kBool, PrefScope::kExtensionSpecific};

// A preference that indicates whether the extension was installed as an
// OEM app.
constexpr PrefMap kWasInstalledByOem = {kPrefWasInstalledByOem, PrefType::kBool,
                                        PrefScope::kExtensionSpecific};

// A preference that indicates when an extension was first installed.
// This preference is created when an extension is installed and deleted when
// it is removed. It is NOT updated when the extension is updated.
constexpr PrefMap kFirstInstallTime = {kPrefFirstInstallTime, PrefType::kTime,
                                       PrefScope::kExtensionSpecific};

// A preference that indicates when an extension was last installed/updated.
constexpr PrefMap kLastUpdateTime = {kPrefLastUpdateTime, PrefType::kTime,
                                     PrefScope::kExtensionSpecific};

bool ReadPrefAsBooleanOrFalse(const ExtensionPrefs* prefs,
                              const ExtensionId& extension_id,
                              const PrefMap& key) {
  bool value = false;
  if (prefs->ReadPrefAsBoolean(extension_id, key, &value)) {
    return value;
  }

  return false;
}

}  // namespace

std::string GetInstallParam(const ExtensionPrefs* prefs,
                            const ExtensionId& extension_id) {
  std::string value;
  // If this fails because the pref isn't set, we return an empty string.
  prefs->ReadPrefAsString(extension_id, kInstallParam, &value);
  return value;
}

void SetInstallParam(ExtensionPrefs* prefs,
                     const ExtensionId& extension_id,
                     std::string value) {
  prefs->SetStringPref(extension_id, kInstallParam, std::move(value));
}

bool IsFromWebStore(const ExtensionPrefs* prefs,
                    const ExtensionId& extension_id) {
  return ReadPrefAsBooleanOrFalse(prefs, extension_id, kFromWebStore);
}

bool WasInstalledByDefault(const ExtensionPrefs* prefs,
                           const ExtensionId& extension_id) {
  return ReadPrefAsBooleanOrFalse(prefs, extension_id, kWasInstalledByDefault);
}

bool WasInstalledByOem(const ExtensionPrefs* prefs,
                       const ExtensionId& extension_id) {
  return ReadPrefAsBooleanOrFalse(prefs, extension_id, kWasInstalledByOem);
}

base::Time GetFirstInstallTime(const ExtensionPrefs* prefs,
                               const ExtensionId& extension_id) {
  return prefs->ReadPrefAsTime(extension_id, kFirstInstallTime);
}

base::Time GetLastUpdateTime(const ExtensionPrefs* prefs,
                             const ExtensionId& extension_id) {
  return prefs->ReadPrefAsTime(extension_id, kLastUpdateTime);
}

}  // namespace extensions
