// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_pairing_floss.h"

#include "base/logging.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"

using device::BluetoothDevice;

namespace floss {

BluetoothPairingFloss::BluetoothPairingFloss(
    BluetoothDevice::PairingDelegate* pairing_delegate)
    : pairing_delegate_(pairing_delegate) {}

BluetoothPairingFloss::~BluetoothPairingFloss() = default;

}  // namespace floss
