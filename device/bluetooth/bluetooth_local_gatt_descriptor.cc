// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"

#include "base/notreached.h"
#include "build/build_config.h"
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    !defined(LINUX_WITHOUT_DBUS)
#include "device/bluetooth/bluez/bluetooth_local_gatt_descriptor_bluez.h"
#include "device/bluetooth/floss/floss_features.h"
#endif

namespace device {

// static
base::WeakPtr<BluetoothLocalGattDescriptor>
BluetoothLocalGattDescriptor::Create(
    const BluetoothUUID& uuid,
    BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristic* characteristic) {
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    !defined(LINUX_WITHOUT_DBUS)
  DCHECK(characteristic);
  if (floss::features::IsFlossEnabled()) {
    // TODO: Floss local gatt work.
    // return (new floss::BluetoothLocalGattDescriptorFloss(
    //             uuid, permissions,
    //             static_cast<floss::BluetoothLocalGattCharacteristicFloss*>(
    //                 characteristic)))
    //     ->weak_ptr_factory_.GetWeakPtr();
    return nullptr;
  } else {
    return (new bluez::BluetoothLocalGattDescriptorBlueZ(
                uuid, permissions,
                static_cast<bluez::BluetoothLocalGattCharacteristicBlueZ*>(
                    characteristic)))
        ->weak_ptr_factory_.GetWeakPtr();
  }
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

BluetoothLocalGattDescriptor::BluetoothLocalGattDescriptor() = default;

BluetoothLocalGattDescriptor::~BluetoothLocalGattDescriptor() = default;

}  // namespace device
