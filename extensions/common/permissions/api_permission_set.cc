// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/api_permission_set.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_info.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace errors = manifest_errors;

namespace {

// Helper object that is implicitly constructible from both a PermissionID and
// from an mojom::APIPermissionID.
struct PermissionIDCompareHelper {
  PermissionIDCompareHelper(const PermissionID& id) : id(id.id()) {}
  PermissionIDCompareHelper(const APIPermissionID id) : id(id) {}

  APIPermissionID id;
};

bool CreateAPIPermission(const std::string& permission_str,
                         const base::Value* permission_value,
                         APIPermissionSet::ParseSource source,
                         APIPermissionSet* api_permissions,
                         std::u16string* error,
                         std::vector<std::string>* unhandled_permissions) {
  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByName(permission_str);
  if (permission_info) {
    if (source != APIPermissionSet::kAllowInternalPermissions &&
        permission_info->is_internal()) {
      // Treat internal permissions as unhandled if we don't allow them. This
      // prevents us from hard erroring in the case that we ever change a
      // permission from internal to not or vice versa.
      if (unhandled_permissions) {
        unhandled_permissions->push_back(permission_str);
      }
      return true;
    }

    std::unique_ptr<APIPermission> permission(
        permission_info->CreateAPIPermission());

    std::string error_details;
    if (!permission->FromValue(permission_value, &error_details,
                               unhandled_permissions)) {
      if (error) {
        if (error_details.empty()) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidPermission,
              permission_info->name());
        } else {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidPermissionWithDetail,
              permission_info->name(),
              error_details);
        }
        return false;
      }
      VLOG(1) << "Parse permission failed.";
    } else {
      api_permissions->insert(std::move(permission));
    }
    return true;
  }

  if (unhandled_permissions)
    unhandled_permissions->push_back(permission_str);
  else
    VLOG(1) << "Unknown permission[" << permission_str << "].";

  return true;
}

bool ParseChildPermissions(const std::string& base_name,
                           const base::Value* permission_value,
                           APIPermissionSet::ParseSource source,
                           APIPermissionSet* api_permissions,
                           std::u16string* error,
                           std::vector<std::string>* unhandled_permissions) {
  if (permission_value) {
    if (!permission_value->is_list()) {
      if (error) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidPermission, base_name);
        return false;
      }
      VLOG(1) << "Permission value is not a list.";
      // Failed to parse, but since error is NULL, failures are not fatal so
      // return true here anyway.
      return true;
    }

    const base::Value::List& list = permission_value->GetList();
    for (size_t i = 0; i < list.size(); ++i) {
      std::string permission_str;
      if (!list[i].is_string()) {
        // permission should be a string
        if (error) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidPermission,
              base_name + '.' + base::NumberToString(i));
          return false;
        }
        VLOG(1) << "Permission is not a string.";
        continue;
      }

      if (!CreateAPIPermission(base_name + '.' + list[i].GetString(), nullptr,
                               source, api_permissions, error,
                               unhandled_permissions))
        return false;
    }
  }

  return CreateAPIPermission(base_name, nullptr, source, api_permissions, error,
                             nullptr);
}

}  // namespace

void APIPermissionSet::insert(APIPermissionID id) {
  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(id);
  DCHECK(permission_info);
  insert(permission_info->CreateAPIPermission());
}

void APIPermissionSet::insert(std::unique_ptr<APIPermission> permission) {
  BaseSetOperators<APIPermissionSet>::insert(std::move(permission));
}

// static
bool APIPermissionSet::ParseFromJSON(
    const base::Value::List& permissions,
    APIPermissionSet::ParseSource source,
    APIPermissionSet* api_permissions,
    std::u16string* error,
    std::vector<std::string>* unhandled_permissions) {
  for (size_t i = 0; i < permissions.size(); ++i) {
    std::string permission_str;
    const base::Value* permission_value = nullptr;
    // permission should be a string or a single key dict.
    if (permissions[i].is_string()) {
      permission_str = permissions[i].GetString();
    } else if (permissions[i].is_dict() &&
               permissions[i].GetDict().size() == 1) {
      auto dict_iter = permissions[i].GetDict().begin();
      permission_str = dict_iter->first;
      permission_value = &dict_iter->second;
    } else {
      if (error) {
        *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidPermission,
                                                     base::NumberToString(i));
        return false;
      }
      VLOG(1) << "Permission is not a string or single key dict.";
      continue;
    }

    // Check if this permission is a special case where its value should
    // be treated as a list of child permissions.
    if (PermissionsInfo::GetInstance()->HasChildPermissions(permission_str)) {
      if (!ParseChildPermissions(permission_str, permission_value, source,
                                 api_permissions, error, unhandled_permissions))
        return false;
      continue;
    }

    if (!CreateAPIPermission(permission_str, permission_value, source,
                             api_permissions, error, unhandled_permissions))
      return false;
  }
  return true;
}

PermissionID::PermissionID(APIPermissionID id)
    : std::pair<APIPermissionID, std::u16string>(id, std::u16string()) {}

PermissionID::PermissionID(APIPermissionID id, const std::u16string& parameter)
    : std::pair<APIPermissionID, std::u16string>(id, parameter) {}

PermissionID::~PermissionID() {
}

PermissionIDSet::PermissionIDSet() {
}

PermissionIDSet::PermissionIDSet(
    std::initializer_list<APIPermissionID> permissions) {
  for (auto permission : permissions) {
    permissions_.insert(PermissionID(permission));
  }
}

PermissionIDSet::PermissionIDSet(const PermissionIDSet& other) = default;

PermissionIDSet::~PermissionIDSet() {
}

void PermissionIDSet::insert(APIPermissionID permission_id) {
  insert(permission_id, std::u16string());
}

void PermissionIDSet::insert(APIPermissionID permission_id,
                             const std::u16string& permission_detail) {
  permissions_.insert(PermissionID(permission_id, permission_detail));
}

void PermissionIDSet::InsertAll(const PermissionIDSet& permission_set) {
  for (const auto& permission : permission_set.permissions_) {
    permissions_.insert(permission);
  }
}

void PermissionIDSet::erase(APIPermissionID permission_id) {
  auto lower_bound = permissions_.lower_bound(PermissionID(permission_id));
  auto upper_bound = lower_bound;
  while (upper_bound != permissions_.end() &&
         upper_bound->id() == permission_id) {
    ++upper_bound;
  }
  permissions_.erase(lower_bound, upper_bound);
}

std::vector<std::u16string> PermissionIDSet::GetAllPermissionParameters()
    const {
  std::vector<std::u16string> params;
  for (const auto& permission : permissions_) {
    params.push_back(permission.parameter());
  }
  return params;
}

bool PermissionIDSet::ContainsID(PermissionID permission_id) const {
  auto it = permissions_.lower_bound(permission_id);
  return it != permissions_.end() && it->id() == permission_id.id();
}

bool PermissionIDSet::ContainsID(APIPermissionID permission_id) const {
  return ContainsID(PermissionID(permission_id));
}

bool PermissionIDSet::ContainsAllIDs(
    const std::set<APIPermissionID>& permission_ids) const {
  return std::includes(permissions_.begin(), permissions_.end(),
                       permission_ids.begin(), permission_ids.end(),
                       [] (const PermissionIDCompareHelper& lhs,
                           const PermissionIDCompareHelper& rhs) {
                         return lhs.id < rhs.id;
                       });
}

bool PermissionIDSet::ContainsAnyID(
    const std::set<APIPermissionID>& permission_ids) const {
  for (APIPermissionID id : permission_ids) {
    if (ContainsID(id))
      return true;
  }
  return false;
}

bool PermissionIDSet::ContainsAnyID(const PermissionIDSet& other) const {
  for (const auto& id : other) {
    if (ContainsID(id))
      return true;
  }
  return false;
}

PermissionIDSet PermissionIDSet::GetAllPermissionsWithID(
    APIPermissionID permission_id) const {
  PermissionIDSet subset;
  auto it = permissions_.lower_bound(PermissionID(permission_id));
  while (it != permissions_.end() && it->id() == permission_id) {
    subset.permissions_.insert(*it);
    ++it;
  }
  return subset;
}

PermissionIDSet PermissionIDSet::GetAllPermissionsWithIDs(
    const std::set<APIPermissionID>& permission_ids) const {
  PermissionIDSet subset;
  for (const auto& permission : permissions_) {
    if (base::Contains(permission_ids, permission.id())) {
      subset.permissions_.insert(permission);
    }
  }
  return subset;
}

bool PermissionIDSet::Includes(const PermissionIDSet& subset) const {
  return base::ranges::includes(permissions_, subset.permissions_);
}

bool PermissionIDSet::Equals(const PermissionIDSet& set) const {
  return permissions_ == set.permissions_;
}

// static
PermissionIDSet PermissionIDSet::Difference(const PermissionIDSet& set_1,
                                            const PermissionIDSet& set_2) {
  return PermissionIDSet(base::STLSetDifference<std::set<PermissionID>>(
      set_1.permissions_, set_2.permissions_));
}

size_t PermissionIDSet::size() const {
  return permissions_.size();
}

bool PermissionIDSet::empty() const {
  return permissions_.empty();
}

PermissionIDSet::PermissionIDSet(const std::set<PermissionID>& permissions)
    : permissions_(permissions) {
}

}  // namespace extensions
