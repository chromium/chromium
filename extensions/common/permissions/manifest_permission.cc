// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/manifest_permission.h"

namespace extensions {

ManifestPermission::ManifestPermission() = default;

ManifestPermission::~ManifestPermission() = default;

std::unique_ptr<ManifestPermission> ManifestPermission::Clone() const {
  return Union(this);
}

bool ManifestPermission::Contains(const ManifestPermission* rhs) const {
  return std::unique_ptr<ManifestPermission>(Intersect(rhs))->Equal(rhs);
}

bool ManifestPermission::Equal(const ManifestPermission* rhs) const {
  return *ToValue() == *rhs->ToValue();
}

bool ManifestPermission::RequiresManagedSessionFullLoginWarning() const {
  return true;
}

}  // namespace extensions
