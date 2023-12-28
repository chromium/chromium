// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_MANIFEST_PERMISSION_SET_H_
#define EXTENSIONS_COMMON_PERMISSIONS_MANIFEST_PERMISSION_SET_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "extensions/common/permissions/base_set_operators.h"

namespace extensions {

class ManifestPermission;
class ManifestPermissionSet;

template<>
struct BaseSetOperatorsTraits<ManifestPermissionSet> {
  using ElementType = ManifestPermission;
  using ElementIDType = std::string;
};

class ManifestPermissionSet : public BaseSetOperators<ManifestPermissionSet> {
 public:
  // Parses permissions from |permissions| and adds the parsed permissions to
  // |manifest_permissions|. If |unhandled_permissions| is not NULL, the names
  // of all permissions that couldn't be parsed will be added to this vector.
  // If |error| is NULL, parsing will continue with the next permission if
  // invalid data is detected. If |error| is not NULL, it will be set to an
  // error message and false is returned when an invalid permission is found.
  static bool ParseFromJSON(const base::Value::List& permissions,
                            ManifestPermissionSet* manifest_permissions,
                            std::u16string* error,
                            std::vector<std::string>* unhandled_permissions);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_MANIFEST_PERMISSION_SET_H_
