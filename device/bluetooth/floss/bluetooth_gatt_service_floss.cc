// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_gatt_service_floss.h"

namespace floss {

BluetoothGattServiceFloss::BluetoothGattServiceFloss(
    BluetoothAdapterFloss* adapter)
    : adapter_(adapter) {}

BluetoothGattServiceFloss::~BluetoothGattServiceFloss() = default;

BluetoothAdapterFloss* BluetoothGattServiceFloss::GetAdapter() const {
  return adapter_;
}

// static
device::BluetoothGattService::GattErrorCode
BluetoothGattServiceFloss::GattStatusToServiceError(const GattStatus status) {
  DCHECK(status != GattStatus::kSuccess);

  // TODO(b/193686564) - Translate remote service gatt errors to correct values.
  return GATT_ERROR_UNKNOWN;
}
}  // namespace floss
