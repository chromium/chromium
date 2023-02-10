// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"

#include "base/notreached.h"
#include "build/build_config.h"
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    !defined(LINUX_WITHOUT_DBUS)
#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/floss_features.h"
#endif

namespace device {

// static
base::WeakPtr<BluetoothLocalGattCharacteristic>
BluetoothLocalGattCharacteristic::Create(const BluetoothUUID& uuid,
                                         Properties properties,
                                         Permissions permissions,
                                         BluetoothLocalGattService* service) {
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    !defined(LINUX_WITHOUT_DBUS)
  DCHECK(service);
  if (floss::features::IsFlossEnabled()) {
    return floss::BluetoothLocalGattCharacteristicFloss::Create(
        uuid, properties, permissions,
        static_cast<floss::BluetoothLocalGattServiceFloss*>(service));
  } else {
    return bluez::BluetoothLocalGattCharacteristicBlueZ::Create(
        uuid, properties, permissions,
        static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(service));
  }
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

BluetoothLocalGattCharacteristic::BluetoothLocalGattCharacteristic() = default;

BluetoothLocalGattCharacteristic::~BluetoothLocalGattCharacteristic() = default;

}  // namespace device
