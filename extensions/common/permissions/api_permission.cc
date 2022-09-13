// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/api_permission.h"

#include "extensions/common/permissions/api_permission_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using extensions::APIPermission;
using extensions::APIPermissionInfo;
using extensions::mojom::APIPermissionID;

class SimpleAPIPermission : public APIPermission {
 public:
  explicit SimpleAPIPermission(const APIPermissionInfo* permission)
    : APIPermission(permission) { }

  ~SimpleAPIPermission() override {}

  extensions::PermissionIDSet GetPermissions() const override {
    extensions::PermissionIDSet permissions;
    permissions.insert(id());
    return permissions;
  }

  bool Check(const APIPermission::CheckParam* param) const override {
    return !param;
  }

  bool Contains(const APIPermission* rhs) const override {
    CHECK_EQ(info(), rhs->info());
    return true;
  }

  bool Equal(const APIPermission* rhs) const override {
    if (this != rhs)
      CHECK_EQ(info(), rhs->info());
    return true;
  }

  bool FromValue(const base::Value* value,
                 std::string* /*error*/,
                 std::vector<std::string>* /*unhandled_permissions*/) override {
    return (value == nullptr);
  }

  std::unique_ptr<base::Value> ToValue() const override { return nullptr; }

  std::unique_ptr<APIPermission> Clone() const override {
    return std::make_unique<SimpleAPIPermission>(info());
  }

  std::unique_ptr<APIPermission> Diff(const APIPermission* rhs) const override {
    CHECK_EQ(info(), rhs->info());
    return nullptr;
  }

  std::unique_ptr<APIPermission> Union(
      const APIPermission* rhs) const override {
    CHECK_EQ(info(), rhs->info());
    return std::make_unique<SimpleAPIPermission>(info());
  }

  std::unique_ptr<APIPermission> Intersect(
      const APIPermission* rhs) const override {
    CHECK_EQ(info(), rhs->info());
    return std::make_unique<SimpleAPIPermission>(info());
  }

};

}  // namespace

namespace extensions {

APIPermission::APIPermission(const APIPermissionInfo* info)
  : info_(info) {
  DCHECK(info_);
}

APIPermission::~APIPermission() { }

mojom::APIPermissionID APIPermission::id() const {
  return info()->id();
}

const char* APIPermission::name() const {
  return info()->name();
}

//
// APIPermissionInfo
//

APIPermissionInfo::APIPermissionInfo(const APIPermissionInfo::InitInfo& info)
    : name_(info.name),
      id_(info.id),
      flags_(info.flags),
      api_permission_constructor_(info.constructor) {}

APIPermissionInfo::~APIPermissionInfo() { }

std::unique_ptr<APIPermission> APIPermissionInfo::CreateAPIPermission() const {
  if (api_permission_constructor_)
    return api_permission_constructor_(this);
  return std::make_unique<SimpleAPIPermission>(this);
}

}  // namespace extensions
