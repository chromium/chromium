// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/permission_set_mojom_traits.h"
#include "base/memory/ptr_util.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/permissions_info.h"

namespace mojo {

bool StructTraits<extensions::mojom::APIPermissionDataView,
                  std::unique_ptr<extensions::APIPermission>>::
    Read(extensions::mojom::APIPermissionDataView data,
         std::unique_ptr<extensions::APIPermission>* out) {
  extensions::mojom::APIPermissionID id;
  if (!data.ReadId(&id))
    return false;

  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(id);
  if (!permission_info)
    return false;

  std::unique_ptr<extensions::APIPermission> api_permission =
      permission_info->CreateAPIPermission();

  std::optional<base::Value> value;
  if (!data.ReadValue(&value))
    return false;

  if (!api_permission->FromValue(value ? &*value : nullptr, nullptr, nullptr))
    return false;

  *out = std::move(api_permission);
  return true;
}

bool StructTraits<extensions::mojom::APIPermissionSetDataView,
                  extensions::APIPermissionSet>::
    Read(extensions::mojom::APIPermissionSetDataView data,
         extensions::APIPermissionSet* out) {
  using MapType = std::map<extensions::mojom::APIPermissionID,
                           std::unique_ptr<extensions::APIPermission>>;
  MapType permissions_map;

  if (!data.ReadPermissionMap(&permissions_map))
    return false;

  for (MapType::iterator it = permissions_map.begin();
       it != permissions_map.end(); ++it) {
    out->insert(std::move(it->second));
  }

  return true;
}

bool StructTraits<extensions::mojom::ManifestPermissionDataView,
                  std::unique_ptr<extensions::ManifestPermission>>::
    Read(extensions::mojom::ManifestPermissionDataView data,
         std::unique_ptr<extensions::ManifestPermission>* out) {
  std::string id;
  if (!data.ReadId(&id))
    return false;

  std::unique_ptr<extensions::ManifestPermission> permission =
      base::WrapUnique(extensions::ManifestHandler::CreatePermission(id));
  if (!permission)
    return false;

  std::optional<base::Value> value;
  if (!data.ReadValue(&value))
    return false;

  if (!permission->FromValue(value ? &*value : nullptr))
    return false;

  *out = std::move(permission);
  return true;
}

bool StructTraits<extensions::mojom::ManifestPermissionSetDataView,
                  extensions::ManifestPermissionSet>::
    Read(extensions::mojom::ManifestPermissionSetDataView data,
         extensions::ManifestPermissionSet* out) {
  using MapType =
      std::map<std::string, std::unique_ptr<extensions::ManifestPermission>>;
  MapType permissions_map;

  if (!data.ReadPermissionMap(&permissions_map))
    return false;

  for (MapType::iterator it = permissions_map.begin();
       it != permissions_map.end(); ++it) {
    out->insert(std::move(it->second));
  }

  return true;
}

bool StructTraits<extensions::mojom::PermissionSetDataView,
                  extensions::PermissionSet>::
    Read(extensions::mojom::PermissionSetDataView data,
         extensions::PermissionSet* out) {
  extensions::APIPermissionSet apis;
  extensions::ManifestPermissionSet manifest_permissions;
  extensions::URLPatternSet hosts;
  extensions::URLPatternSet user_script_hosts;
  if (!data.ReadApis(&apis) ||
      !data.ReadManifestPermissions(&manifest_permissions) ||
      !data.ReadHosts(&hosts) || !data.ReadUserScriptHosts(&user_script_hosts))
    return false;

  *out = extensions::PermissionSet(
      std::move(apis), std::move(manifest_permissions), std::move(hosts),
      std::move(user_script_hosts));

  return true;
}

}  // namespace mojo
