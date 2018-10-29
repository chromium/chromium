// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_EXTENSIONS_API_PERMISSIONS_H_
#define EXTENSIONS_COMMON_PERMISSIONS_EXTENSIONS_API_PERMISSIONS_H_

#include "base/containers/span.h"
#include "extensions/common/alias.h"
#include "extensions/common/permissions/api_permission.h"

namespace extensions {
namespace api_permissions {

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos();
base::span<const Alias> GetPermissionAliases();

}  // namespace api_permissions
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_EXTENSIONS_API_PERMISSIONS_H_
