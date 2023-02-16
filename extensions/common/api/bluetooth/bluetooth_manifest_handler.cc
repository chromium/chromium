// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/bluetooth/bluetooth_manifest_handler.h"

#include "extensions/common/api/bluetooth/bluetooth_manifest_data.h"
#include "extensions/common/api/bluetooth/bluetooth_manifest_permission.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

BluetoothManifestHandler::BluetoothManifestHandler() = default;

BluetoothManifestHandler::~BluetoothManifestHandler() = default;

bool BluetoothManifestHandler::Parse(Extension* extension,
                                     std::u16string* error) {
  const base::Value* bluetooth =
      extension->manifest()->FindPath(manifest_keys::kBluetooth);
  CHECK(bluetooth != nullptr);
  std::unique_ptr<BluetoothManifestData> data =
      BluetoothManifestData::FromValue(*bluetooth, error);
  if (!data)
    return false;

  extension->SetManifestData(manifest_keys::kBluetooth, std::move(data));
  return true;
}

ManifestPermission* BluetoothManifestHandler::CreatePermission() {
  return new BluetoothManifestPermission();
}

ManifestPermission* BluetoothManifestHandler::CreateInitialRequiredPermission(
    const Extension* extension) {
  BluetoothManifestData* data = BluetoothManifestData::Get(extension);
  if (data)
    return data->permission()->Clone().release();
  return nullptr;
}

base::span<const char* const> BluetoothManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kBluetooth};
  return kKeys;
}

}  // namespace extensions
