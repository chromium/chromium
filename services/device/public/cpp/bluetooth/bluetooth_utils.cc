// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"

namespace device {

const BluetoothUUID& GetSerialPortProfileUUID() {
  // The Serial Port Profile (SPP) UUID is 1101.
  // https://chromium-review.googlesource.com/c/chromium/src/+/2334682/17..19
  static const BluetoothUUID kValue("1101");
  return kValue;
}

}  // namespace device
