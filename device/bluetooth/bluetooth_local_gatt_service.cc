// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_service.h"

#include "build/build_config.h"
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    !defined(LINUX_WITHOUT_DBUS)
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"
#include "device/bluetooth/floss/floss_features.h"
#endif

namespace device {

// static
base::WeakPtr<BluetoothLocalGattService> BluetoothLocalGattService::Create(
    BluetoothAdapter* adapter,
    const BluetoothUUID& uuid,
    bool is_primary,
    BluetoothLocalGattService* included_service,
    BluetoothLocalGattService::Delegate* delegate) {
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    !defined(LINUX_WITHOUT_DBUS)
  if (floss::features::IsFlossEnabled()) {
    // TODO: Floss local gatt work.
    // return (new floss::BluetoothLocalGattServiceFloss(
    //             static_cast<floss::BluetoothAdapterFloss*>(adapter), uuid,
    //             is_primary, delegate))
    //     ->weak_ptr_factory_.GetWeakPtr();
    return nullptr;
  } else {
    return (new bluez::BluetoothLocalGattServiceBlueZ(
                static_cast<bluez::BluetoothAdapterBlueZ*>(adapter), uuid,
                is_primary, delegate))
        ->weak_ptr_factory_.GetWeakPtr();
  }
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

BluetoothLocalGattService::BluetoothLocalGattService() = default;

BluetoothLocalGattService::~BluetoothLocalGattService() = default;

}  // namespace device
