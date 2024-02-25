// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_SOCKETS_SOCKETS_MANIFEST_PERMISSION_H_
#define EXTENSIONS_COMMON_API_SOCKETS_SOCKETS_MANIFEST_PERMISSION_H_

#include <set>
#include <vector>

#include "extensions/common/install_warning.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/socket_permission_entry.h"

namespace content {
struct SocketPermissionRequest;
}

namespace extensions {
class Extension;
}

namespace extensions {

using SocketPermissionEntrySet = std::set<SocketPermissionEntry>;

class SocketsManifestPermission : public ManifestPermission {
 public:
  SocketsManifestPermission();
  ~SocketsManifestPermission() override;

  // Tries to construct the info based on |value|, as it would have appeared in
  // the manifest. Sets |error| and returns an empty scoped_ptr on failure.
  static std::unique_ptr<SocketsManifestPermission> FromValue(
      const base::Value& value,
      std::u16string* error);

  bool CheckRequest(const Extension* extension,
                    const content::SocketPermissionRequest& request) const;

  void AddPermission(const SocketPermissionEntry& entry);

  // extensions::ManifestPermission overrides.
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
  bool RequiresManagedSessionFullLoginWarning() const override;

  const SocketPermissionEntrySet& entries() const { return permissions_; }

  // Adds the permissions from |sockets| into the permission lists |ids| and
  // |messages|. If either is NULL, that list is ignored.
  static void AddSocketHostPermissions(const SocketPermissionEntrySet& sockets,
                                       PermissionIDSet* ids);

  SocketPermissionEntrySet permissions_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_SOCKETS_SOCKETS_MANIFEST_PERMISSION_H_
