// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_H_

#include "base/functional/callback.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"

namespace device {

class MockBluetoothLowEnergyScanSession : public BluetoothLowEnergyScanSession {
 public:
  explicit MockBluetoothLowEnergyScanSession(
      base::OnceClosure destructor_callback);
  ~MockBluetoothLowEnergyScanSession() override;

 private:
  base::OnceClosure destructor_callback_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_H_
