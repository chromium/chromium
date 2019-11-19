// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_notify_session.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

namespace device {

BluetoothGattNotifySession::BluetoothGattNotifySession(
    base::WeakPtr<BluetoothRemoteGattCharacteristic> characteristic)
    : characteristic_(characteristic),
      characteristic_id_(characteristic.get() ? characteristic->GetIdentifier()
                                              : std::string()),
      active_(true) {}

BluetoothGattNotifySession::~BluetoothGattNotifySession() {
  if (active_) {
    Stop(base::DoNothing());
  }
}

std::string BluetoothGattNotifySession::GetCharacteristicIdentifier() const {
  return characteristic_id_;
}

BluetoothRemoteGattCharacteristic*
BluetoothGattNotifySession::GetCharacteristic() const {
  return characteristic_.get();
}

bool BluetoothGattNotifySession::IsActive() {
  return active_ && characteristic_ != nullptr &&
         characteristic_->IsNotifying();
}

void BluetoothGattNotifySession::Stop(base::OnceClosure callback) {
  active_ = false;
  if (characteristic_ != nullptr) {
    characteristic_->StopNotifySession(this, std::move(callback));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  }
}

}  // namespace device
