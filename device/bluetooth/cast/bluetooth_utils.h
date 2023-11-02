// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CAST_BLUETOOTH_UTILS_H_
#define DEVICE_BLUETOOTH_CAST_BLUETOOTH_UTILS_H_

#include <string>

#include "chromecast/public/bluetooth/bluetooth_types.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

// This file contains common utilities for implementing Chromium bluetooth
// interfaces with the Cast Bluetooth stack.
namespace device {

// Return |addr| in the canonical format used by Chromium Bluetooth code,
// which is a 48-bit mac address with strictly uppercase digits (ex.
// "AA:BB:CC:DD:EE:FF"). Any class implementing a Bluetooth interface which
// needs to reference an address should use this function to obtain the correct
// string.
std::string DEVICE_BLUETOOTH_EXPORT
GetCanonicalBluetoothAddress(const chromecast::bluetooth_v2_shlib::Addr& addr);

// Convert |uuid| to BluetoothUUID, the type used by Chromium Bluetooth code.
device::BluetoothUUID DEVICE_BLUETOOTH_EXPORT
UuidToBluetoothUUID(const chromecast::bluetooth_v2_shlib::Uuid& uuid);

// Return |uuid| in the canonical format used by Chromium Bluetooth code,
// which is a 128-bit lowercase uuid:
// This is the same as calling UuidToBluetoothUUID(uuid).canonical_value().
std::string DEVICE_BLUETOOTH_EXPORT
GetCanonicalBluetoothUuid(const chromecast::bluetooth_v2_shlib::Uuid& uuid);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CAST_BLUETOOTH_UTILS_H_