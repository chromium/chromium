// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_PERMISSION_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_PERMISSION_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/manifest_permission.h"

namespace extensions {

// Manages permissions for each MIME type a third-party mime handler is
// allowed to claim via the "mime_types_handler" manifest key. Each MIME
// type maps to its own warning, so set operations and re-consent work per
// type.
class MimeTypesHandlerPermission : public ManifestPermission {
 public:
  using MimeTypeSet = base::flat_set<std::string>;

  // Returns the permission whose warning covers claiming `mime_type`, or
  // nullopt when extensions are not allowed to handle it.
  static std::optional<mojom::APIPermissionID> PermissionIDForMimeType(
      const std::string_view& mime_type);

  MimeTypesHandlerPermission();
  explicit MimeTypesHandlerPermission(MimeTypeSet mime_types);
  ~MimeTypesHandlerPermission() override;

  // ManifestPermission:
  std::string name() const override;
  std::string id() const override;
  PermissionIDSet GetPermissions() const override;
  bool FromValue(const base::Value* value) override;
  std::unique_ptr<base::Value> ToValue() const override;
  std::unique_ptr<ManifestPermission> Diff(
      const ManifestPermission* rhs) const override;
  std::unique_ptr<ManifestPermission> Union(
      const ManifestPermission* rhs) const override;
  std::unique_ptr<ManifestPermission> Intersect(
      const ManifestPermission* rhs) const override;
  bool RequiresManagementUIWarning() const override;

  const MimeTypeSet& mime_types() const { return mime_types_; }

 private:
  MimeTypeSet mime_types_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_PERMISSION_H_
