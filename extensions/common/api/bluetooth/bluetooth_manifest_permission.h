// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_BLUETOOTH_BLUETOOTH_MANIFEST_PERMISSION_H_
#define EXTENSIONS_COMMON_API_BLUETOOTH_BLUETOOTH_MANIFEST_PERMISSION_H_

#include <set>
#include <vector>

#include "extensions/common/install_warning.h"
#include "extensions/common/permissions/manifest_permission.h"

namespace extensions {
class Extension;
}

namespace extensions {
struct BluetoothPermissionRequest;
}

namespace extensions {

class BluetoothManifestPermission : public ManifestPermission {
 public:
  using BluetoothUuidSet = std::set<std::string>;
  BluetoothManifestPermission();
  ~BluetoothManifestPermission() override;

  // Tries to construct the info based on |value|, as it would have appeared in
  // the manifest. Sets |error| and returns an empty scoped_ptr on failure.
  static std::unique_ptr<BluetoothManifestPermission> FromValue(
      const base::Value& value,
      std::u16string* error);

  bool CheckRequest(const Extension* extension,
                    const BluetoothPermissionRequest& request) const;
  bool CheckSocketPermitted(const Extension* extension) const;
  bool CheckLowEnergyPermitted(const Extension* extension) const;
  bool CheckPeripheralPermitted(const Extension* extension) const;

  void AddPermission(const std::string& uuid);

  // extensions::ManifestPermission overrides.
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

  const BluetoothUuidSet& uuids() const {
    return uuids_;
  }

 private:
  BluetoothUuidSet uuids_;
  bool socket_;
  bool low_energy_;
  bool peripheral_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_BLUETOOTH_BLUETOOTH_MANIFEST_PERMISSION_H_
