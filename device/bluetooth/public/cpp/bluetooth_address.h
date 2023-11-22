// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_ADDRESS_H_
#define DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_ADDRESS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

#include "base/containers/span.h"

namespace device {

// Parses a Bluetooth address to an output buffer. The output buffer must be
// exactly 6 bytes in size. The address can be formatted in one of three ways:
//
//   1A:2B:3C:4D:5E:6F
//   1A-2B-3C-4D-5E-6F
//   1A2B3C4D5E6F
bool ParseBluetoothAddress(std::string_view input, base::span<uint8_t> output);

// Returns |address| in the canonical format: XX:XX:XX:XX:XX:XX, where each 'X'
// is a hex digit.  If the input |address| is invalid, returns an empty string.
std::string CanonicalizeBluetoothAddress(std::string_view address);
std::string CanonicalizeBluetoothAddress(
    const std::array<uint8_t, 6>& address_bytes);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_ADDRESS_H_
