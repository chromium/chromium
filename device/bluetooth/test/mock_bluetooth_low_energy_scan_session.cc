// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_low_energy_scan_session.h"

namespace device {

MockBluetoothLowEnergyScanSession::MockBluetoothLowEnergyScanSession(
    base::OnceClosure destructor_callback)
    : destructor_callback_(std::move(destructor_callback)) {}

MockBluetoothLowEnergyScanSession::~MockBluetoothLowEnergyScanSession() {
  std::move(destructor_callback_).Run();
}

}  // namespace device
