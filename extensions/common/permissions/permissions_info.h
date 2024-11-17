// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_INFO_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_INFO_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"

namespace extensions {

struct Alias;

// A global object that holds the extension permission instances and provides
// methods for accessing them.
class PermissionsInfo {
 public:
  PermissionsInfo(const PermissionsInfo&) = delete;
  PermissionsInfo& operator=(const PermissionsInfo&) = delete;

  static PermissionsInfo* GetInstance();

  // Registers the permissions specified by |infos| along with the
  // |aliases|.
  void RegisterPermissions(base::span<const APIPermissionInfo::InitInfo> infos,
                           base::span<const Alias> aliases);

  // Returns the permission with the given |id|, and NULL if it doesn't exist.
  const APIPermissionInfo* GetByID(mojom::APIPermissionID id) const;

  // Returns the permission with the given |name|, and NULL if none
  // exists.
  const APIPermissionInfo* GetByName(const std::string& name) const;

  // Returns a set containing all valid api permission ids.
  APIPermissionSet GetAllForTest() const;

  // Converts all the permission names in |permission_names| to permission ids.
  APIPermissionSet GetAllByNameForTest(
      const std::set<std::string>& permission_names) const;

  // Checks if any permissions have names that start with |name| followed by a
  // period.
  bool HasChildPermissions(const std::string& name) const;

  // Gets the total number of API permissions.
  size_t get_permission_count() const { return permission_count_; }

 private:
  friend struct base::LazyInstanceTraitsBase<PermissionsInfo>;

  PermissionsInfo();

  virtual ~PermissionsInfo();

  // Registers an |alias| for a given permission |name|.
  void RegisterAlias(const Alias& alias);

  // Registers a permission with the specified attributes and flags.
  void RegisterPermission(std::unique_ptr<APIPermissionInfo> permission);

  // Maps permission ids to permissions. Owns the permissions.
  using IDMap = base::flat_map<mojom::APIPermissionID,
                               std::unique_ptr<APIPermissionInfo>>;

  // Maps names and aliases to permissions. Doesn't own the permissions.
  using NameMap = std::map<std::string, APIPermissionInfo*>;

  IDMap id_map_;
  NameMap name_map_;

  size_t permission_count_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_INFO_H_
