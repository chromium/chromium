// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_PERMISSIONS_PARSER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_PERMISSIONS_PARSER_H_

#include <memory>
#include <string>

#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_set.h"

namespace extensions {

class Extension;
class URLPatternSet;

// The class for parsing the kPermissions and kOptionalPermissions keys in the
// manifest. Because permissions are slightly different than other keys (they
// are used in many different handlers and need to be the first and last key
// touched), this is not an actual ManifestHandler (hence the difference in
// name).
class PermissionsParser {
 public:
  PermissionsParser();
  ~PermissionsParser();

  // Parse the manifest-specified permissions.
  bool Parse(Extension* extension, std::u16string* error);

  // Finalize the permissions, setting the related manifest data on the
  // extension.
  void Finalize(Extension* extension);

  // Modify the manifest permissions. These methods should only be used
  // during initialization and will DCHECK() for safety.
  static void AddAPIPermission(Extension* extension,
                               mojom::APIPermissionID permission);
  static void AddAPIPermission(Extension* extension, APIPermission* permission);
  static bool HasAPIPermission(const Extension* extension,
                               mojom::APIPermissionID permission);
  static void SetScriptableHosts(Extension* extension,
                                 const URLPatternSet& scriptable_hosts);

  // Return the extension's manifest-specified permissions. In no cases should
  // these permissions be used to determine if an action is allowed. Instead,
  // use PermissionsData.
  static const PermissionSet& GetRequiredPermissions(
      const Extension* extension);
  static const PermissionSet& GetOptionalPermissions(
      const Extension* extension);

 private:
  struct InitialPermissions;

  // The initial permissions for the extension, which can still be modified.
  std::unique_ptr<InitialPermissions> initial_required_permissions_;
  std::unique_ptr<InitialPermissions> initial_optional_permissions_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_PERMISSIONS_PARSER_H_
