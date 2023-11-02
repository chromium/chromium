// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_fuchsia.h"

namespace device {

BluetoothTestFuchsia::BluetoothTestFuchsia() = default;

BluetoothTestFuchsia::~BluetoothTestFuchsia() = default;

bool BluetoothTestFuchsia::PlatformSupportsLowEnergy() {
  return true;
}

}  // namespace device
