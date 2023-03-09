// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/manifest_permission_set.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/manifest_permission.h"

namespace {

using extensions::ErrorUtils;
using extensions::ManifestPermission;
using extensions::ManifestPermissionSet;
using extensions::ManifestHandler;
namespace errors = extensions::manifest_errors;

bool CreateManifestPermission(const std::string& permission_name,
                              const base::Value* permission_value,
                              ManifestPermissionSet* manifest_permissions,
                              std::u16string* error,
                              std::vector<std::string>* unhandled_permissions) {
  std::unique_ptr<ManifestPermission> permission(
      ManifestHandler::CreatePermission(permission_name));

  if (!permission) {
    if (unhandled_permissions)
      unhandled_permissions->push_back(permission_name);
    else
      LOG(WARNING) << "Unknown permission[" << permission_name << "].";
    return true;
  }

  if (!permission->FromValue(permission_value)) {
    if (error) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidPermission, permission_name);
      return false;
    }
    LOG(WARNING) << "Parse permission failed.";
    return true;
  } else {
    manifest_permissions->insert(std::move(permission));
    return true;
  }
}

}  // namespace

namespace extensions {

// static
bool ManifestPermissionSet::ParseFromJSON(
    const base::Value::List& permissions,
    ManifestPermissionSet* manifest_permissions,
    std::u16string* error,
    std::vector<std::string>* unhandled_permissions) {
  for (size_t i = 0; i < permissions.size(); ++i) {
    const base::Value& value = permissions[i];
    std::string permission_name;
    const base::Value* permission_value = nullptr;
    // Permission `value` should be a string or a single key dict.
    if (value.is_string()) {
      permission_name = value.GetString();
    } else if (value.is_dict() && value.GetDict().size() == 1u) {
      auto dict_iter = value.GetDict().begin();
      permission_name = dict_iter->first;
      permission_value = &dict_iter->second;
    } else {
      if (error) {
        *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidPermission,
                                                     base::NumberToString(i));
        return false;
      }
      LOG(WARNING) << "Permission is not a string or single key dict.";
      continue;
    }

    if (!CreateManifestPermission(permission_name, permission_value,
                                  manifest_permissions, error,
                                  unhandled_permissions))
      return false;
  }
  return true;
}

}  // namespace extensions
