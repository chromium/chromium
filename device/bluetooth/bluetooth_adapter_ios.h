// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_IOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_IOS_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_low_energy_adapter_apple.h"

namespace device {

class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterIOS
    : public BluetoothLowEnergyAdapterApple {
 public:
  static scoped_refptr<BluetoothAdapterIOS> CreateAdapter();
  static scoped_refptr<BluetoothAdapterIOS> CreateAdapterForTest(
      std::string name,
      std::string address,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  BluetoothAdapterIOS(const BluetoothAdapterIOS&) = delete;
  BluetoothAdapterIOS& operator=(const BluetoothAdapterIOS&) = delete;

 protected:
  friend class BluetoothLowEnergyAdapterAppleTest;

  // BluetoothAdapter override:
  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override;

  // BluetoothLowEnergyAdapterApple override:
  base::WeakPtr<BluetoothLowEnergyAdapterApple> GetLowEnergyWeakPtr() override;
  void TriggerSystemPermissionPrompt() override;

 private:
  BluetoothAdapterIOS();
  ~BluetoothAdapterIOS() override;

  base::WeakPtrFactory<BluetoothAdapterIOS> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_IOS_H_
