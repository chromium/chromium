// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"

namespace device {

#if (!BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)) || \
    defined(LINUX_WITHOUT_DBUS)
// static
base::WeakPtr<BluetoothLocalGattCharacteristic>
BluetoothLocalGattCharacteristic::Create(const BluetoothUUID& uuid,
                                         Properties properties,
                                         Permissions permissions,
                                         BluetoothLocalGattService* service) {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif

BluetoothLocalGattCharacteristic::BluetoothLocalGattCharacteristic() = default;

BluetoothLocalGattCharacteristic::~BluetoothLocalGattCharacteristic() = default;

}  // namespace device
