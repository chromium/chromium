// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_SETTINGS_OVERRIDE_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_SETTINGS_OVERRIDE_PERMISSION_H_

#include <string>

#include "extensions/common/permissions/api_permission.h"

namespace extensions {

// Takes care of creating custom permission messages for extensions that
// override settings.
class SettingsOverrideAPIPermission : public APIPermission {
 public:
  explicit SettingsOverrideAPIPermission(const APIPermissionInfo* permission);
  SettingsOverrideAPIPermission(const APIPermissionInfo* permission,
                                const std::string& setting_value);
  ~SettingsOverrideAPIPermission() override;

  // APIPermission overrides.
  PermissionIDSet GetPermissions() const override;
  bool Check(const APIPermission::CheckParam* param) const override;
  bool Contains(const APIPermission* rhs) const override;
  bool Equal(const APIPermission* rhs) const override;
  bool FromValue(const base::Value* value,
                 std::string* error,
                 std::vector<std::string>* unhandled_permissions) override;
  std::unique_ptr<base::Value> ToValue() const override;
  std::unique_ptr<APIPermission> Clone() const override;
  std::unique_ptr<APIPermission> Diff(const APIPermission* rhs) const override;
  std::unique_ptr<APIPermission> Union(const APIPermission* rhs) const override;
  std::unique_ptr<APIPermission> Intersect(
      const APIPermission* rhs) const override;

 private:
  std::string setting_value_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_SETTINGS_OVERRIDE_PERMISSION_H_
