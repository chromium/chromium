// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/bluetooth/bluetooth_manifest_permission.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "extensions/common/api/bluetooth/bluetooth_manifest_data.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ipc/ipc_message.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace bluetooth_errors {
const char kErrorInvalidUuid[] = "Invalid UUID '*'";
}

namespace {

bool ParseUuid(BluetoothManifestPermission* permission,
               const std::string& uuid,
               std::u16string* error) {
  device::BluetoothUUID bt_uuid(uuid);
  if (!bt_uuid.IsValid()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        bluetooth_errors::kErrorInvalidUuid, uuid);
    return false;
  }
  permission->AddPermission(uuid);
  return true;
}

bool ParseUuidArray(BluetoothManifestPermission* permission,
                    const std::vector<std::string>& uuids,
                    std::u16string* error) {
  for (const auto& uuid : uuids) {
    if (!ParseUuid(permission, uuid, error)) {
      return false;
    }
  }
  return true;
}

}  // namespace

BluetoothManifestPermission::BluetoothManifestPermission()
    : socket_(false), low_energy_(false), peripheral_(false) {
}

BluetoothManifestPermission::~BluetoothManifestPermission() = default;

// static
std::unique_ptr<BluetoothManifestPermission>
BluetoothManifestPermission::FromValue(const base::Value& value,
                                       std::u16string* error) {
  auto bluetooth = api::extensions_manifest_types::Bluetooth::FromValue(value);
  if (!bluetooth.has_value()) {
    *error = std::move(bluetooth).error();
    return nullptr;
  }

  std::unique_ptr<BluetoothManifestPermission> result(
      new BluetoothManifestPermission());
  if (bluetooth->uuids) {
    if (!ParseUuidArray(result.get(), *bluetooth->uuids, error)) {
      return nullptr;
    }
  }
  if (bluetooth->socket) {
    result->socket_ = *(bluetooth->socket);
  }
  if (bluetooth->low_energy) {
    result->low_energy_ = *(bluetooth->low_energy);
  }
  if (bluetooth->peripheral) {
    result->peripheral_ = *(bluetooth->peripheral);
  }
  return result;
}

bool BluetoothManifestPermission::CheckRequest(
    const Extension* extension,
    const BluetoothPermissionRequest& request) const {

  device::BluetoothUUID param_uuid(request.uuid);
  for (auto it = uuids_.cbegin(); it != uuids_.cend(); ++it) {
    device::BluetoothUUID uuid(*it);
    if (param_uuid == uuid)
      return true;
  }
  return false;
}

bool BluetoothManifestPermission::CheckSocketPermitted(
    const Extension* extension) const {
  return socket_;
}

bool BluetoothManifestPermission::CheckLowEnergyPermitted(
    const Extension* extension) const {
  return low_energy_;
}

bool BluetoothManifestPermission::CheckPeripheralPermitted(
    const Extension* extension) const {
  return peripheral_;
}

std::string BluetoothManifestPermission::name() const {
  return manifest_keys::kBluetooth;
}

std::string BluetoothManifestPermission::id() const { return name(); }

PermissionIDSet BluetoothManifestPermission::GetPermissions() const {
  PermissionIDSet permissions;
  permissions.insert(mojom::APIPermissionID::kBluetooth);
  if (!uuids_.empty()) {
    permissions.insert(mojom::APIPermissionID::kBluetoothDevices);
  }
  return permissions;
}

bool BluetoothManifestPermission::FromValue(const base::Value* value) {
  if (!value)
    return false;
  std::u16string error;
  std::unique_ptr<BluetoothManifestPermission> manifest_permission(
      BluetoothManifestPermission::FromValue(*value, &error));

  if (!manifest_permission)
    return false;

  uuids_ = manifest_permission->uuids_;
  return true;
}

std::unique_ptr<base::Value> BluetoothManifestPermission::ToValue() const {
  api::extensions_manifest_types::Bluetooth bluetooth;
  bluetooth.uuids.emplace(uuids_.begin(), uuids_.end());
  return std::make_unique<base::Value>(bluetooth.ToValue());
}

std::unique_ptr<ManifestPermission> BluetoothManifestPermission::Diff(
    const ManifestPermission* rhs) const {
  const BluetoothManifestPermission* other =
      static_cast<const BluetoothManifestPermission*>(rhs);

  auto result = std::make_unique<BluetoothManifestPermission>();
  result->uuids_ = base::STLSetDifference<BluetoothUuidSet>(
      uuids_, other->uuids_);
  return result;
}

std::unique_ptr<ManifestPermission> BluetoothManifestPermission::Union(
    const ManifestPermission* rhs) const {
  const BluetoothManifestPermission* other =
      static_cast<const BluetoothManifestPermission*>(rhs);

  auto result = std::make_unique<BluetoothManifestPermission>();
  result->uuids_ = base::STLSetUnion<BluetoothUuidSet>(
      uuids_, other->uuids_);
  return result;
}

std::unique_ptr<ManifestPermission> BluetoothManifestPermission::Intersect(
    const ManifestPermission* rhs) const {
  const BluetoothManifestPermission* other =
      static_cast<const BluetoothManifestPermission*>(rhs);

  auto result = std::make_unique<BluetoothManifestPermission>();
  result->uuids_ = base::STLSetIntersection<BluetoothUuidSet>(
      uuids_, other->uuids_);
  return result;
}

void BluetoothManifestPermission::AddPermission(const std::string& uuid) {
  uuids_.insert(uuid);
}

bool BluetoothManifestPermission::RequiresManagementUIWarning() const {
  return false;
}

}  // namespace extensions
