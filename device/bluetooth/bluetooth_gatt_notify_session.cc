// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_notify_session.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

namespace device {

// static
BluetoothGattNotifySession::Id BluetoothGattNotifySession::GetNextId() {
  static Id::Generator generator;
  return generator.GenerateNextId();
}

BluetoothGattNotifySession::BluetoothGattNotifySession(
    base::WeakPtr<BluetoothRemoteGattCharacteristic> characteristic)
    : characteristic_(characteristic),
      characteristic_id_(characteristic.get() ? characteristic->GetIdentifier()
                                              : std::string()),
      active_(true),
      unique_id_(GetNextId()) {}

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
    characteristic_->StopNotifySession(unique_id(), std::move(callback));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
}

}  // namespace device
