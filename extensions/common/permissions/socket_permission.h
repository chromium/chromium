// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_SOCKET_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_SOCKET_PERMISSION_H_

#include <stdint.h>

#include <string>

#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/set_disjunction_permission.h"
#include "extensions/common/permissions/socket_permission_data.h"

namespace extensions {

class SocketPermission
    : public SetDisjunctionPermission<SocketPermissionData, SocketPermission> {
 public:
  struct CheckParam : APIPermission::CheckParam {
    CheckParam(content::SocketPermissionRequest::OperationType type,
               const std::string& host,
               uint16_t port)
        : request(type, host, port) {}
    content::SocketPermissionRequest request;
  };

  explicit SocketPermission(const APIPermissionInfo* info);

  ~SocketPermission() override;

  // SetDisjunctionPermission overrides.
  bool FromValue(const base::Value* value,
                 std::string* error,
                 std::vector<std::string>* unhandled_permissions) override;

  // APIPermission overrides
  PermissionIDSet GetPermissions() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_SOCKET_PERMISSION_H_
