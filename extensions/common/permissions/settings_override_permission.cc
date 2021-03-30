// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/settings_override_permission.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

SettingsOverrideAPIPermission::SettingsOverrideAPIPermission(
    const APIPermissionInfo* permission)
    : APIPermission(permission) {}

SettingsOverrideAPIPermission::SettingsOverrideAPIPermission(
    const APIPermissionInfo* permission,
    const std::string& setting_value)
    : APIPermission(permission), setting_value_(setting_value) {}

SettingsOverrideAPIPermission::~SettingsOverrideAPIPermission() {}

PermissionIDSet SettingsOverrideAPIPermission::GetPermissions() const {
  PermissionIDSet permissions;
  permissions.insert(info()->id(), base::UTF8ToUTF16(setting_value_));
  return permissions;
}

bool SettingsOverrideAPIPermission::Check(
    const APIPermission::CheckParam* param) const {
  return (param == NULL);
}

bool SettingsOverrideAPIPermission::Contains(const APIPermission* rhs) const {
  CHECK_EQ(info(), rhs->info());
  return true;
}

bool SettingsOverrideAPIPermission::Equal(const APIPermission* rhs) const {
  if (this != rhs)
    CHECK_EQ(info(), rhs->info());
  return true;
}

bool SettingsOverrideAPIPermission::FromValue(
    const base::Value* value,
    std::string* /*error*/,
    std::vector<std::string>* unhandled_permissions) {
  return value && value->GetAsString(&setting_value_);
}

std::unique_ptr<base::Value> SettingsOverrideAPIPermission::ToValue() const {
  return std::make_unique<base::Value>(setting_value_);
}

std::unique_ptr<APIPermission> SettingsOverrideAPIPermission::Clone() const {
  return std::make_unique<SettingsOverrideAPIPermission>(info(),
                                                         setting_value_);
}

std::unique_ptr<APIPermission> SettingsOverrideAPIPermission::Diff(
    const APIPermission* rhs) const {
  CHECK_EQ(info(), rhs->info());
  return nullptr;
}

std::unique_ptr<APIPermission> SettingsOverrideAPIPermission::Union(
    const APIPermission* rhs) const {
  CHECK_EQ(info(), rhs->info());
  return std::make_unique<SettingsOverrideAPIPermission>(info(),
                                                         setting_value_);
}

std::unique_ptr<APIPermission> SettingsOverrideAPIPermission::Intersect(
    const APIPermission* rhs) const {
  CHECK_EQ(info(), rhs->info());
  return std::make_unique<SettingsOverrideAPIPermission>(info(),
                                                         setting_value_);
}

void SettingsOverrideAPIPermission::Write(base::Pickle* m) const {}

bool SettingsOverrideAPIPermission::Read(const base::Pickle* m,
                                         base::PickleIterator* iter) {
  return true;
}

void SettingsOverrideAPIPermission::Log(std::string* log) const {
  *log = setting_value_;
}

}  // namespace extensions
