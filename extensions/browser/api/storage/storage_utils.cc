// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_utils.h"

#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions::storage_utils {

namespace {

constexpr PrefMap kPrefSessionStorageAccessLevel = {
    "storage_session_access_level", PrefType::kInteger,
    PrefScope::kExtensionSpecific};

}  // namespace

api::storage::AccessLevel GetSessionAccessLevel(
    const ExtensionId& extension_id,
    content::BrowserContext& browser_context) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(&browser_context);

  // Default access level is only secure contexts.
  int access_level =
      base::to_underlying(api::storage::AccessLevel::kTrustedContexts);
  prefs->ReadPrefAsInteger(extension_id, kPrefSessionStorageAccessLevel,
                           &access_level);

  // Return access level iff it's a valid value.
  if (access_level > 0 &&
      access_level <=
          base::to_underlying(api::storage::AccessLevel::kMaxValue)) {
    return static_cast<api::storage::AccessLevel>(access_level);
  }

  // Otherwise, return the default session access level.
  return api::storage::AccessLevel::kTrustedContexts;
}

void SetSessionAccessLevel(const ExtensionId& extension_id,
                           content::BrowserContext& browser_context,
                           api::storage::AccessLevel access_level) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(&browser_context);
  prefs->SetIntegerPref(extension_id, kPrefSessionStorageAccessLevel,
                        base::to_underlying(access_level));
}

base::Value ValueChangeToValue(
    std::vector<SessionStorageManager::ValueChange> changes) {
  base::Value::Dict changes_value;
  for (auto& change : changes) {
    base::Value::Dict change_value;
    if (change.old_value.has_value()) {
      change_value.Set("oldValue", std::move(change.old_value.value()));
    }
    if (change.new_value) {
      change_value.Set("newValue", change.new_value->Clone());
    }
    changes_value.Set(change.key, std::move(change_value));
  }
  return base::Value(std::move(changes_value));
}

}  // namespace extensions::storage_utils
