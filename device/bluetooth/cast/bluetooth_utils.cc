// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_utils.h"

#include "base/strings/string_piece.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

std::string GetCanonicalBluetoothAddress(
    const chromecast::bluetooth_v2_shlib::Addr& addr) {
  return device::BluetoothDevice::CanonicalizeAddress(
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
