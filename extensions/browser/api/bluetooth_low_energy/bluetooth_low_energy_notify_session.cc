// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_notify_session.h"

#include "base/lazy_instance.h"

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<BluetoothLowEnergyNotifySession>>>::DestructorAtExit
    g_factory = LAZY_INSTANCE_INITIALIZER;

template <>
BrowserContextKeyedAPIFactory<
    ApiResourceManager<BluetoothLowEnergyNotifySession>>*
ApiResourceManager<BluetoothLowEnergyNotifySession>::GetFactoryInstance() {
  return g_factory.Pointer();
}

BluetoothLowEnergyNotifySession::BluetoothLowEnergyNotifySession(
    bool persistent,
    const std::string& owner_extension_id,
    std::unique_ptr<device::BluetoothGattNotifySession> session)
    : ApiResource(owner_extension_id),
      persistent_(persistent),
      session_(session.release()) {}

BluetoothLowEnergyNotifySession::~BluetoothLowEnergyNotifySession() = default;

device::BluetoothGattNotifySession*
BluetoothLowEnergyNotifySession::GetSession() const {
  return session_.get();
}

bool BluetoothLowEnergyNotifySession::IsPersistent() const {
  return persistent_;
}

}  // namespace extensions
