// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"

#include "base/notreached.h"
#include "build/build_config.h"

namespace device {

#if (!BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)) || \
    defined(LINUX_WITHOUT_DBUS)
// static
base::WeakPtr<BluetoothLocalGattDescriptor>
BluetoothLocalGattDescriptor::Create(
    const BluetoothUUID& uuid,
    BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristic* characteristic) {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif

BluetoothLocalGattDescriptor::BluetoothLocalGattDescriptor() = default;

BluetoothLocalGattDescriptor::~BluetoothLocalGattDescriptor() = default;

}  // namespace device
