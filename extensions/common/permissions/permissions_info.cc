// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permissions_info.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "extensions/common/alias.h"

namespace extensions {

static base::LazyInstance<PermissionsInfo>::Leaky g_permissions_info =
    LAZY_INSTANCE_INITIALIZER;

// static
PermissionsInfo* PermissionsInfo::GetInstance() {
  return g_permissions_info.Pointer();
}

void PermissionsInfo::RegisterPermissions(
    base::span<const APIPermissionInfo::InitInfo> infos,
    base::span<const Alias> aliases) {
  for (const auto& info : infos)
    RegisterPermission(base::WrapUnique(new APIPermissionInfo((info))));

  for (const auto& alias : aliases)
    RegisterAlias(alias);
}

const APIPermissionInfo* PermissionsInfo::GetByID(
    mojom::APIPermissionID id) const {
  auto i = id_map_.find(id);
  return (i == id_map_.end()) ? nullptr : i->second.get();
}

const APIPermissionInfo* PermissionsInfo::GetByName(
    const std::string& name) const {
  auto i = name_map_.find(name);
  return (i == name_map_.end()) ? nullptr : i->second;
}

APIPermissionSet PermissionsInfo::GetAllForTest() const {
  APIPermissionSet permissions;
  for (auto i = id_map_.cbegin(); i != id_map_.cend(); ++i)
    permissions.insert(i->second->id());
  return permissions;
}

APIPermissionSet PermissionsInfo::GetAllByNameForTest(
    const std::set<std::string>& permission_names) const {
  APIPermissionSet permissions;
  for (auto i = permission_names.cbegin(); i != permission_names.cend(); ++i) {
    const APIPermissionInfo* permission_info = GetByName(*i);
    if (permission_info) {
      permissions.insert(permission_info->id());
    }
  }
  return permissions;
}

bool PermissionsInfo::HasChildPermissions(const std::string& name) const {
  auto i = name_map_.lower_bound(name + '.');
  if (i == name_map_.end()) return false;
  return base::StartsWith(i->first, name + '.', base::CompareCase::SENSITIVE);
}

PermissionsInfo::PermissionsInfo()
    : permission_count_(0) {
}

PermissionsInfo::~PermissionsInfo() {
}

void PermissionsInfo::RegisterAlias(const Alias& alias) {
  DCHECK(base::Contains(name_map_, alias.real_name));
  DCHECK(!base::Contains(name_map_, alias.name));
  name_map_[alias.name] = name_map_[alias.real_name];
}

void PermissionsInfo::RegisterPermission(
    std::unique_ptr<APIPermissionInfo> permission) {
  DCHECK(!base::Contains(id_map_, permission->id()));
  DCHECK(!base::Contains(name_map_, permission->name()));

  name_map_[permission->name()] = permission.get();
  id_map_[permission->id()] = std::move(permission);

  permission_count_++;
}

}  // namespace extensions
