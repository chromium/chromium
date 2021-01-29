// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WIN_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_low_energy_defs_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class BluetoothRemoteGattCharacteristicWin;
class BluetoothTaskManagerWin;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattDescriptorWin
    : public BluetoothRemoteGattDescriptor {
 public:
  BluetoothRemoteGattDescriptorWin(
      BluetoothRemoteGattCharacteristicWin* parent_characteristic,
      BTH_LE_GATT_DESCRIPTOR* descriptor_info,
      scoped_refptr<base::SequencedTaskRunner>& ui_task_runner);
  ~BluetoothRemoteGattDescriptorWin() override;

  // Override BluetoothRemoteGattDescriptor interfaces.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  BluetoothRemoteGattCharacteristic::Permissions GetPermissions()
      const override;
  void ReadRemoteDescriptor(ValueCallback callback,
                            ErrorCallback error_callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& new_value,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

  uint16_t GetAttributeHandle() const;
  PBTH_LE_GATT_DESCRIPTOR GetWinDescriptorInfo() const {
    return descriptor_info_.get();
  }

 private:
  BluetoothRemoteGattCharacteristicWin* parent_characteristic_;
  std::unique_ptr<BTH_LE_GATT_DESCRIPTOR> descriptor_info_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  base::FilePath service_path_;
  scoped_refptr<BluetoothTaskManagerWin> task_manager_;
  BluetoothRemoteGattCharacteristic::Permissions descriptor_permissions_;
  BluetoothUUID descriptor_uuid_;
  std::string descriptor_identifier_;
  std::vector<uint8_t> descriptor_value_;

  base::WeakPtrFactory<BluetoothRemoteGattDescriptorWin> weak_ptr_factory_{
      this};
  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattDescriptorWin);
};

}  // namespace device.
#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WIN_H_
