// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_ios.h"

namespace device {

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapter::CreateAdapter() {
  return BluetoothAdapterIOS::CreateAdapter();
}

// static
scoped_refptr<BluetoothAdapterIOS> BluetoothAdapterIOS::CreateAdapter() {
  return base::WrapRefCounted(new BluetoothAdapterIOS());
}

// static
scoped_refptr<BluetoothAdapterIOS> BluetoothAdapterIOS::CreateAdapterForTest(
    std::string name,
    std::string address,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  auto adapter = base::WrapRefCounted(new BluetoothAdapterIOS());
  adapter->InitForTest(ui_task_runner);  // IN-TEST
  return adapter;
}

BluetoothAdapterIOS::BluetoothAdapterIOS() = default;

BluetoothAdapterIOS::~BluetoothAdapterIOS() = default;

base::WeakPtr<BluetoothAdapter> BluetoothAdapterIOS::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<BluetoothLowEnergyAdapterApple>
BluetoothAdapterIOS::GetLowEnergyWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BluetoothAdapterIOS::TriggerSystemPermissionPrompt() {
  // TODO(crbug.com/346409873): Find the system API to trigger prompt for iOS.
}

}  // namespace device
