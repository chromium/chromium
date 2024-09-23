// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_EMULATION_FAKE_REMOTE_GATT_DESCRIPTOR_H_
#define DEVICE_BLUETOOTH_EMULATION_FAKE_REMOTE_GATT_DESCRIPTOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/emulation/fake_read_response.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace bluetooth {

// Implements device::BluetoothRemoteGattDescriptors. Meant to be used
// by FakeRemoteGattCharacteristic to keep track of the descriptor's state and
// attributes.
//
// Not intended for direct use by clients.  See README.md.
class FakeRemoteGattDescriptor : public device::BluetoothRemoteGattDescriptor {
 public:
  FakeRemoteGattDescriptor(
      const std::string& descriptor_id,
      const device::BluetoothUUID& descriptor_uuid,
      device::BluetoothRemoteGattCharacteristic* characteristic);
  ~FakeRemoteGattDescriptor() override;

  // If |gatt_code| is mojom::kGATTSuccess the next read request will call
  // its success callback with |value|. Otherwise it will call its error
  // callback.
  void SetNextReadResponse(uint16_t gatt_code,
                           const std::optional<std::vector<uint8_t>>& value);

  // If |gatt_code| is mojom::kGATTSuccess the next write request will call its
  // success callback. Otherwise it will call its error callback.
  void SetNextWriteResponse(uint16_t gatt_code);

  // Returns the last successfully written value to the descriptor. Returns
  // nullopt if no value has been written yet.
  const std::optional<std::vector<uint8_t>>& last_written_value() {
    return last_written_value_;
  }

  // Returns true if there are no pending responses for this descriptor.
  bool AllResponsesConsumed();

  // device::BluetoothGattDescriptor overrides:
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  device::BluetoothRemoteGattCharacteristic::Permissions GetPermissions()
      const override;

  // device::BluetoothRemoteGattDescriptor overrides:
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  void ReadRemoteDescriptor(ValueCallback callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& value,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

 private:
  void DispatchReadResponse(ValueCallback callback);

  void DispatchWriteResponse(base::OnceClosure callback,
                             ErrorCallback error_callback,
                             const std::vector<uint8_t>& value);

  const std::string descriptor_id_;
  const device::BluetoothUUID descriptor_uuid_;
  raw_ptr<device::BluetoothRemoteGattCharacteristic> characteristic_;
  std::vector<uint8_t> value_;

  // Last successfully written value to the descriptor.
  std::optional<std::vector<uint8_t>> last_written_value_;

  // Used to decide which callback should be called when
  // ReadRemoteDescriptor is called.
  std::optional<FakeReadResponse> next_read_response_;

  // Used to decide which callback should be called when WriteRemoteDescriptor
  // is called.
  std::optional<uint16_t> next_write_response_;

  base::WeakPtrFactory<FakeRemoteGattDescriptor> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_EMULATION_FAKE_REMOTE_GATT_DESCRIPTOR_H_
