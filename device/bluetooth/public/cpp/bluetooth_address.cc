// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/public/cpp/bluetooth_address.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace device {

bool ParseBluetoothAddress(std::string_view input, base::span<uint8_t> output) {
  if (output.size() != 6)
    return false;

  // Try parsing addresses that lack separators, like "1A2B3C4D5E6F".
  if (input.size() == 12)
    return base::HexStringToSpan(input, output);

  // Try parsing MAC address with separators like: "00:11:22:33:44:55" or
  // "00-11-22-33-44-55". Separator can be either '-' or ':', but must use the
  // same style throughout.
  if (input.size() == 17) {
    const char separator = input[2];
    if (separator != '-' && separator != ':')
      return false;
    return (input[2] == separator) && (input[5] == separator) &&
           (input[8] == separator) && (input[11] == separator) &&
           (input[14] == separator) &&
           base::HexStringToSpan(input.substr(0, 2), output.subspan<0, 1>()) &&
           base::HexStringToSpan(input.substr(3, 2), output.subspan<1, 1>()) &&
           base::HexStringToSpan(input.substr(6, 2), output.subspan<2, 1>()) &&
           base::HexStringToSpan(input.substr(9, 2), output.subspan<3, 1>()) &&
           base::HexStringToSpan(input.substr(12, 2), output.subspan<4, 1>()) &&
           base::HexStringToSpan(input.substr(15, 2), output.subspan<5, 1>());
  }

  return false;
}

std::string CanonicalizeBluetoothAddress(std::string_view address) {
  std::array<uint8_t, 6> bytes;

  if (!ParseBluetoothAddress(address, bytes))
    return std::string();

  return CanonicalizeBluetoothAddress(bytes);
}

std::string CanonicalizeBluetoothAddress(
    const std::array<uint8_t, 6>& address_bytes) {
  return base::StringPrintf(
      "%02X:%02X:%02X:%02X:%02X:%02X", address_bytes[0], address_bytes[1],
      address_bytes[2], address_bytes[3], address_bytes[4], address_bytes[5]);
}

}  // namespace device
