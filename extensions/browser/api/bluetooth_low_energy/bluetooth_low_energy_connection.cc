// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_connection.h"

#include "base/lazy_instance.h"

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<BluetoothLowEnergyConnection>>>::DestructorAtExit
    g_factory = LAZY_INSTANCE_INITIALIZER;

template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<BluetoothLowEnergyConnection>>*
ApiResourceManager<BluetoothLowEnergyConnection>::GetFactoryInstance() {
  return g_factory.Pointer();
}

BluetoothLowEnergyConnection::BluetoothLowEnergyConnection(
    bool persistent,
    const std::string& owner_extension_id,
    std::unique_ptr<device::BluetoothGattConnection> connection)
    : ApiResource(owner_extension_id),
      persistent_(persistent),
      connection_(connection.release()) {}

BluetoothLowEnergyConnection::~BluetoothLowEnergyConnection() = default;

device::BluetoothGattConnection* BluetoothLowEnergyConnection::GetConnection()
    const {
  return connection_.get();
}

bool BluetoothLowEnergyConnection::IsPersistent() const {
  return persistent_;
}

}  // namespace extensions
