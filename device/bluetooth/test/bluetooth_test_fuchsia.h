// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_FUCHSIA_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_FUCHSIA_H_

#include "device/bluetooth/test/bluetooth_test.h"

namespace device {

class BluetoothTestFuchsia : public BluetoothTestBase {
 public:
  BluetoothTestFuchsia();
  ~BluetoothTestFuchsia() override;

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
using BluetoothTest = BluetoothTestFuchsia;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_FUCHSIA_H_
