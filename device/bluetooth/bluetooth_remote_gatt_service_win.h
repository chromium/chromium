// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WIN_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "device/bluetooth/bluetooth_low_energy_defs_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

namespace device {

class BluetoothAdapterWin;
class BluetoothDeviceWin;
class BluetoothRemoteGattCharacteristicWin;
class BluetoothTaskManagerWin;

// The BluetoothRemoteGattServiceWin class implements BluetoothRemoteGattService
// for remote GATT services on Windows 8 and later.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattServiceWin
    : public BluetoothRemoteGattService {
 public:
  BluetoothRemoteGattServiceWin(
      BluetoothDeviceWin* device,
      base::FilePath service_path,
      BluetoothUUID service_uuid,
      uint16_t service_attribute_handle,
      bool is_primary,
      BluetoothRemoteGattServiceWin* parent_service,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  ~BluetoothRemoteGattServiceWin() override;

  // Override BluetoothRemoteGattService interfaces.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;
  BluetoothDevice* GetDevice() const override;
  std::vector<BluetoothRemoteGattService*> GetIncludedServices() const override;

  // Notify |characteristic| discovery complete, |characteristic| is the
  // included characteritic of this service.
  void GattCharacteristicDiscoveryComplete(
      BluetoothRemoteGattCharacteristicWin* characteristic);

  // Update included services and characteristics.
  void Update();
  uint16_t GetAttributeHandle() const { return service_attribute_handle_; }
  base::FilePath GetServicePath() { return service_path_; }
  BluetoothAdapterWin* GetWinAdapter() { return adapter_; }

 private:
  void OnGetIncludedCharacteristics(
      std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC> characteristics,
      uint16_t num,
      HRESULT hr);
  void UpdateIncludedCharacteristics(
      PBTH_LE_GATT_CHARACTERISTIC characteristics,
      uint16_t num);

  // Sends GattServiceDiscoveryComplete notification if necessary.
  void NotifyGattServiceDiscoveryCompleteIfNecessary();

  // Checks if the characteristic with |uuid| and |attribute_handle| has already
  // been discovered as included characteristic.
  bool IsCharacteristicDiscovered(const BTH_LE_UUID& uuid,
                                  uint16_t attribute_handle);

  // Checks if |characteristic| still exists in this service according to newly
  // retreived |num| of included |characteristics|.
  static bool DoesCharacteristicExist(
      PBTH_LE_GATT_CHARACTERISTIC characteristics,
      uint16_t num,
      BluetoothRemoteGattCharacteristicWin* characteristic);

  void RemoveIncludedCharacteristic(std::string identifier);
  void ClearIncludedCharacteristics();

  BluetoothAdapterWin* adapter_;
  BluetoothDeviceWin* device_;
  base::FilePath service_path_;
  BluetoothUUID service_uuid_;
  uint16_t service_attribute_handle_;
  bool is_primary_;
  BluetoothRemoteGattServiceWin* parent_service_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  std::string service_identifier_;

  // BluetoothTaskManagerWin to handle asynchronously Bluetooth IO and platform
  // dependent operations.
  scoped_refptr<BluetoothTaskManagerWin> task_manager_;

  // The element of the set is the identifier of
  // BluetoothRemoteGattCharacteristicWin instance.
  std::set<std::string> discovery_completed_included_characteristics_;

  // Flag indicates if discovery complete notification has been send out to
  // avoid duplicate notification.
  bool discovery_complete_notified_ = false;

  // Counts the number of asynchronous operations that are discovering
  // characteristics.
  int discovery_pending_count_ = 0;

  base::WeakPtrFactory<BluetoothRemoteGattServiceWin> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattServiceWin);
};

}  // namespace device.
#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WIN_H_
