// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_utils.h"

#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace device {

std::string GetCanonicalBluetoothAddress(
    const chromecast::bluetooth_v2_shlib::Addr& addr) {
  return device::CanonicalizeBluetoothAddress(
      chromecast::bluetooth::util::AddrToString(addr));
}

BluetoothUUID UuidToBluetoothUUID(
    const chromecast::bluetooth_v2_shlib::Uuid& uuid) {
  return BluetoothUUID(chromecast::bluetooth::util::UuidToString(uuid));
}

std::string GetCanonicalBluetoothUuid(
    const chromecast::bluetooth_v2_shlib::Uuid& uuid) {
  return UuidToBluetoothUUID(uuid).canonical_value();
}

}  // namespace device
