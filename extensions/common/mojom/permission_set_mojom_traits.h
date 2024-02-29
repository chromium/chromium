// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MOJOM_PERMISSION_SET_MOJOM_TRAITS_H_
#define EXTENSIONS_COMMON_MOJOM_PERMISSION_SET_MOJOM_TRAITS_H_

#include <optional>

#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/mojom/permission_set.mojom-shared.h"
#include "extensions/common/mojom/url_pattern_set_mojom_traits.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<extensions::mojom::APIPermissionDataView,
                    std::unique_ptr<extensions::APIPermission>> {
  static extensions::mojom::APIPermissionID id(
      const std::unique_ptr<extensions::APIPermission>& permission) {
    return permission->id();
  }

  static std::optional<base::Value> value(
      const std::unique_ptr<extensions::APIPermission>& permission) {
    std::unique_ptr<base::Value> value = permission->ToValue();

    if (value == nullptr)
      return std::nullopt;
    return base::Value::FromUniquePtrValue(std::move(value));
  }

  static bool Read(extensions::mojom::APIPermissionDataView data,
                   std::unique_ptr<extensions::APIPermission>* out);
};

template <>
struct StructTraits<extensions::mojom::APIPermissionSetDataView,
                    extensions::APIPermissionSet> {
  static const std::map<extensions::mojom::APIPermissionID,
                        std::unique_ptr<extensions::APIPermission>>&
  permission_map(const extensions::APIPermissionSet& set) {
    return set.map();
  }

  static bool Read(extensions::mojom::APIPermissionSetDataView data,
                   extensions::APIPermissionSet* out);
};

template <>
struct StructTraits<extensions::mojom::ManifestPermissionDataView,
                    std::unique_ptr<extensions::ManifestPermission>> {
  static std::string id(
      const std::unique_ptr<extensions::ManifestPermission>& permission) {
    return permission->id();
  }

  static std::optional<base::Value> value(
      const std::unique_ptr<extensions::ManifestPermission>& permission) {
    std::unique_ptr<base::Value> value = permission->ToValue();

    if (value == nullptr)
      return std::nullopt;
    return base::Value::FromUniquePtrValue(std::move(value));
  }

  static bool Read(extensions::mojom::ManifestPermissionDataView data,
                   std::unique_ptr<extensions::ManifestPermission>* out);
};

template <>
struct StructTraits<extensions::mojom::ManifestPermissionSetDataView,
                    extensions::ManifestPermissionSet> {
  static const std::map<std::string,
                        std::unique_ptr<extensions::ManifestPermission>>&
  permission_map(const extensions::ManifestPermissionSet& set) {
    return set.map();
  }

  static bool Read(extensions::mojom::ManifestPermissionSetDataView data,
                   extensions::ManifestPermissionSet* out);
};

template <>
struct StructTraits<extensions::mojom::PermissionSetDataView,
                    extensions::PermissionSet> {
  static const extensions::APIPermissionSet& apis(
      const extensions::PermissionSet& set) {
    return set.apis();
  }

  static const extensions::ManifestPermissionSet& manifest_permissions(
      const extensions::PermissionSet& set) {
    return set.manifest_permissions();
  }

  static const extensions::URLPatternSet& hosts(
      const extensions::PermissionSet& set) {
    return set.explicit_hosts();
  }

  static const extensions::URLPatternSet& user_script_hosts(
      const extensions::PermissionSet& set) {
    return set.scriptable_hosts();
  }

  static bool Read(extensions::mojom::PermissionSetDataView data,
                   extensions::PermissionSet* out);
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MOJOM_PERMISSION_SET_MOJOM_TRAITS_H_
