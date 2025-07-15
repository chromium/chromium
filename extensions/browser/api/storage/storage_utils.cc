// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_utils.h"

#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions::storage_utils {

namespace {

constexpr PrefMap kPrefSessionStorageAccessLevel = {
    "storage_session_access_level", PrefType::kInteger,
    PrefScope::kExtensionSpecific};

constexpr PrefMap kPrefLocalStorageAccessLevel = {
    "storage_local_access_level", PrefType::kInteger,
    PrefScope::kExtensionSpecific};

constexpr PrefMap kPrefSyncStorageAccessLevel = {"storage_sync_access_level",
                                                 PrefType::kInteger,
                                                 PrefScope::kExtensionSpecific};

constexpr PrefMap kPrefManagedStorageAccessLevel = {
    "storage_managed_access_level", PrefType::kInteger,
    PrefScope::kExtensionSpecific};

const PrefMap* GetPrefMapForStorageArea(StorageAreaNamespace storage_area) {
  switch (storage_area) {
    case StorageAreaNamespace::kSession:
      return &kPrefSessionStorageAccessLevel;
    case StorageAreaNamespace::kLocal:
      return &kPrefLocalStorageAccessLevel;
    case StorageAreaNamespace::kSync:
      return &kPrefSyncStorageAccessLevel;
    case StorageAreaNamespace::kManaged:
      return &kPrefManagedStorageAccessLevel;
    // An invalid storage area does not have access levels.
    case StorageAreaNamespace::kInvalid:
      NOTREACHED();
  }
}
}  // namespace

api::storage::AccessLevel GetAccessLevelForArea(
    const ExtensionId& extension_id,
    content::BrowserContext& browser_context,
    StorageAreaNamespace storage_area) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(&browser_context);
  const PrefMap* pref_map = GetPrefMapForStorageArea(storage_area);

  int stored_access_level_int = 0;
  if (pref_map && prefs->ReadPrefAsInteger(extension_id, *pref_map,
                                           &stored_access_level_int)) {
    // Return access level iff it's a valid value.
    if (stored_access_level_int > 0 &&
        stored_access_level_int <=
            base::to_underlying(api::storage::AccessLevel::kMaxValue)) {
      return static_cast<api::storage::AccessLevel>(stored_access_level_int);
    }
  }

  // Otherwise, return the default access level for the specified storage area.
  switch (storage_area) {
    case StorageAreaNamespace::kSession:
      return api::storage::AccessLevel::kTrustedContexts;
    case StorageAreaNamespace::kLocal:
    case StorageAreaNamespace::kSync:
    case StorageAreaNamespace::kManaged:
      return api::storage::AccessLevel::kTrustedAndUntrustedContexts;
    case StorageAreaNamespace::kInvalid:
      NOTREACHED();
  }
}

void SetAccessLevelForArea(const ExtensionId& extension_id,
                           content::BrowserContext& browser_context,
                           StorageAreaNamespace storage_area,
                           api::storage::AccessLevel access_level) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(&browser_context);
  const PrefMap* pref_map = GetPrefMapForStorageArea(storage_area);
  prefs->SetIntegerPref(extension_id, *pref_map,
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

// TODO(crbug.com/41034787): Use this to limit renderer access to storage to
// cases where there has been an injection.
bool CanRendererAccessExtensionStorage(
    content::BrowserContext& browser_context,
    const Extension& extension,
    std::optional<StorageAreaNamespace> storage_area,
    content::RenderFrameHost* render_frame_host,
    content::RenderProcessHost& render_process_host) {
  if (!extension.permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kStorage)) {
    return false;
  }

  if (storage_area == StorageAreaNamespace::kSession) {
    if (extension.manifest_version() < 3) {
      return false;
    }

    api::storage::AccessLevel access_level =
        GetAccessLevelForArea(extension.id(), browser_context, *storage_area);
    if (access_level == api::storage::AccessLevel::kTrustedContexts) {
      ProcessMap* process_map = ProcessMap::Get(&browser_context);
      return process_map->IsPrivilegedExtensionProcess(
          extension, render_process_host.GetDeprecatedID());
    }
  }

  return util::CanRendererActOnBehalfOfExtension(
      extension.id(), render_frame_host, render_process_host,
      /*include_user_scripts=*/false);
}

}  // namespace extensions::storage_utils
