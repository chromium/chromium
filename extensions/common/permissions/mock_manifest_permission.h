// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_MOCK_MANIFEST_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_MOCK_MANIFEST_PERMISSION_H_

#include <memory>
#include <string>

#include "extensions/common/permissions/manifest_permission.h"

namespace base {
class Value;
}

namespace extensions {

// Useful for mocking ManifestPermission in tests.
class MockManifestPermission : public ManifestPermission {
 public:
  explicit MockManifestPermission(const std::string& name);
  explicit MockManifestPermission(const std::string& name,
                                  const std::string& value);

  MockManifestPermission(const MockManifestPermission&) = delete;
  MockManifestPermission& operator=(const MockManifestPermission&) = delete;

  std::string name() const override;
  std::string id() const override;

  PermissionIDSet GetPermissions() const override;

  bool FromValue(const base::Value* value) override;
  std::unique_ptr<base::Value> ToValue() const override;

  std::unique_ptr<ManifestPermission> Diff(
      const ManifestPermission* rhs) const override;
  std::unique_ptr<ManifestPermission> Union(
      const ManifestPermission* rhs) const override;
  std::unique_ptr<ManifestPermission> Intersect(
      const ManifestPermission* rhs) const override;
  bool RequiresManagementUIWarning() const override;

 private:
  std::string name_;
  // value_ is ignored for the Diff, Union and Intersect operations
  std::string value_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_MOCK_MANIFEST_PERMISSION_H_
