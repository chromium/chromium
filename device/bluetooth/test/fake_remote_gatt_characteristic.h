// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_CHARACTERISTIC_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_CHARACTERISTIC_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/public/mojom/test/fake_bluetooth.mojom.h"
#include "device/bluetooth/test/fake_read_response.h"
#include "device/bluetooth/test/fake_remote_gatt_descriptor.h"

namespace device {
class BluetoothRemoteGattService;
class BluetoothRemoteGattDescriptor;
}  // namespace device

namespace bluetooth {

// Implements device::BluetoothRemoteGattCharacteristics. Meant to be used
// by FakeRemoteGattService to keep track of the characteristic's state and
// attributes.
//
// Not intended for direct use by clients.  See README.md.
class FakeRemoteGattCharacteristic
    : public device::BluetoothRemoteGattCharacteristic {
 public:
  FakeRemoteGattCharacteristic(const std::string& characteristic_id,
                               const device::BluetoothUUID& characteristic_uuid,
                               mojom::CharacteristicPropertiesPtr properties,
                               device::BluetoothRemoteGattService* service);
  ~FakeRemoteGattCharacteristic() override;

  // Adds a fake descriptor with |descriptor_uuid| to this characteristic.
  // Returns the descriptor's Id.
  std::string AddFakeDescriptor(const device::BluetoothUUID& descriptor_uuid);

  // Removes a fake descriptor with |identifier| from this characteristic.
  bool RemoveFakeDescriptor(const std::string& identifier);

  // If |gatt_code| is mojom::kGATTSuccess the next read request will call
  // its success callback with |value|. Otherwise it will call its error
  // callback.
  void SetNextReadResponse(uint16_t gatt_code,
                           const base::Optional<std::vector<uint8_t>>& value);

  // If |gatt_code| is mojom::kGATTSuccess the next write with response request
  // will call its success callback. Otherwise it will call its error callback.
  void SetNextWriteResponse(uint16_t gatt_code);

  // If |gatt_code| is mojom::kGATTSuccess the next subscribe to notifications
  // with response request will call its success callback.  Otherwise it will
  // call its error callback.
  void SetNextSubscribeToNotificationsResponse(uint16_t gatt_code);

  // If |gatt_code| is mojom::kGATTSuccess the next unsubscribe to notifications
  // with response request will call its success callback.  Otherwise it will
  // call its error callback.
  void SetNextUnsubscribeFromNotificationsResponse(uint16_t gatt_code);

  // Returns true if there are no pending responses for this characteristc or
  // any of its descriptors.
  bool AllResponsesConsumed();

  // Returns the last sucessfully written value to the characteristic. Returns
  // nullopt if no value has been written yet.
  const base::Optional<std::vector<uint8_t>>& last_written_value() {
    return last_written_value_;
  }

  // device::BluetoothGattCharacteristic overrides:
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // device::BluetoothRemoteGattCharacteristic overrides:
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothRemoteGattService* GetService() const override;
  void ReadRemoteCharacteristic(ValueCallback callback,
                                ErrorCallback error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override;
#if defined(OS_CHROMEOS)
  void PrepareWriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                        base::OnceClosure callback,
                                        ErrorCallback error_callback) override;
#endif
  bool WriteWithoutResponse(base::span<const uint8_t> value) override;

 protected:
#if defined(OS_CHROMEOS)
  // device::BluetoothRemoteGattCharacteristic overrides:
  void SubscribeToNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      NotificationType notification_type,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
#else
  // device::BluetoothRemoteGattCharacteristic overrides:
  void SubscribeToNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
#endif
  void UnsubscribeFromNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

 private:
  void DispatchReadResponse(ValueCallback callback,
                            ErrorCallback error_callback);
  void DispatchWriteResponse(base::OnceClosure callback,
                             ErrorCallback error_callback,
                             const std::vector<uint8_t>& value);
  void DispatchSubscribeToNotificationsResponse(base::OnceClosure callback,
                                                ErrorCallback error_callback);
  void DispatchUnsubscribeFromNotificationsResponse(
      base::OnceClosure callback,
      ErrorCallback error_callback);

  const std::string characteristic_id_;
  const device::BluetoothUUID characteristic_uuid_;
  Properties properties_;
  device::BluetoothRemoteGattService* service_;
  std::vector<uint8_t> value_;

  // Last successfully written value to the characteristic.
  base::Optional<std::vector<uint8_t>> last_written_value_;

  // Used to decide which callback should be called when
  // ReadRemoteCharacteristic is called.
  base::Optional<FakeReadResponse> next_read_response_;

  // Used to decide which callback should be called when
  // WriteRemoteCharacteristic is called.
  base::Optional<uint16_t> next_write_response_;

  // Used to decide which callback should be called when
  // SubscribeToNotifications is called.
  base::Optional<uint16_t> next_subscribe_to_notifications_response_;

  // Used to decide which callback should be called when
  // UnsubscribeFromNotifications is called.
  base::Optional<uint16_t> next_unsubscribe_from_notifications_response_;

  size_t last_descriptor_id_;

  base::WeakPtrFactory<FakeRemoteGattCharacteristic> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_CHARACTERISTIC_H_
