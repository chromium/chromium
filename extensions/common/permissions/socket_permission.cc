// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/socket_permission.h"

#include <algorithm>
#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "extensions/common/api/sockets/sockets_manifest_permission.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/set_disjunction_permission.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Extracts the SocketPermissionEntry fields from a set of SocketPermissionData,
// and places them in their own set. Useful for converting the
// std::set<SocketPermissionEntry> field from SocketPermission into a parameter
// that can be passed to SocketsManifestPermission::AddSocketHostPermissions().
SocketPermissionEntrySet ExtractSocketEntries(
    const std::set<SocketPermissionData>& data_set) {
  SocketPermissionEntrySet entries;
  for (const auto& data : data_set)
    entries.insert(data.entry());
  return entries;
}

}  // namespace

SocketPermission::SocketPermission(const APIPermissionInfo* info)
    : SetDisjunctionPermission<SocketPermissionData, SocketPermission>(info) {}

SocketPermission::~SocketPermission() = default;

bool SocketPermission::FromValue(
    const base::Value* value,
    std::string* error,
    std::vector<std::string>* unhandled_permissions) {
  bool parsed_ok = SetDisjunctionPermission<
      SocketPermissionData, SocketPermission>::FromValue(value, error,
                                                         unhandled_permissions);
  if (parsed_ok && data_set_.empty()) {
    if (error)
      *error = "NULL or empty permission list";
    return false;
  }
  return parsed_ok;
}

PermissionIDSet SocketPermission::GetPermissions() const {
  PermissionIDSet ids;
  SocketPermissionEntrySet entries = ExtractSocketEntries(data_set_);
  SocketsManifestPermission::AddSocketHostPermissions(entries, &ids);
  return ids;
}

}  // namespace extensions
