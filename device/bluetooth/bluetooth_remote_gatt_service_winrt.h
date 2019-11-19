// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <wrl/client.h>

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_winrt.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

class BluetoothDevice;
class BluetoothGattDiscovererWinrt;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattServiceWinrt
    : public BluetoothRemoteGattService {
 public:
  static std::unique_ptr<BluetoothRemoteGattServiceWinrt> Create(
      BluetoothDevice* device,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattDeviceService>
          gatt_service);
  ~BluetoothRemoteGattServiceWinrt() override;

  // BluetoothRemoteGattService:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;
  BluetoothDevice* GetDevice() const override;
  std::vector<BluetoothRemoteGattService*> GetIncludedServices() const override;

  void UpdateCharacteristics(BluetoothGattDiscovererWinrt* gatt_discoverer);

  ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::IGattDeviceService*
  GetDeviceServiceForTesting();

  template <typename Interface>
  static GattErrorCode GetGattErrorCode(Interface* i) {
    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IReference<uint8_t>>
        protocol_error_ref;
    HRESULT hr = i->get_ProtocolError(&protocol_error_ref);
    if (FAILED(hr)) {
      VLOG(2) << "Getting Protocol Error Reference failed: "
              << logging::SystemErrorCodeToString(hr);
      return GattErrorCode::GATT_ERROR_UNKNOWN;
    }

    if (!protocol_error_ref) {
      VLOG(2) << "Got Null Protocol Error Reference.";
      return GattErrorCode::GATT_ERROR_UNKNOWN;
    }

    uint8_t protocol_error;
    hr = protocol_error_ref->get_Value(&protocol_error);
    if (FAILED(hr)) {
      VLOG(2) << "Getting Protocol Error Value failed: "
              << logging::SystemErrorCodeToString(hr);
      return GattErrorCode::GATT_ERROR_UNKNOWN;
    }

    VLOG(2) << "Got Protocol Error: " << static_cast<int>(protocol_error);

    // GATT Protocol Errors are described in the Bluetooth Core Specification
    // Version 5.0 Vol 3, Part F, 3.4.1.1.
    switch (protocol_error) {
      case 0x01:  // Invalid Handle
        return GATT_ERROR_FAILED;
      case 0x02:  // Read Not Permitted
        return GATT_ERROR_NOT_PERMITTED;
      case 0x03:  // Write Not Permitted
        return GATT_ERROR_NOT_PERMITTED;
      case 0x04:  // Invalid PDU
        return GATT_ERROR_FAILED;
      case 0x05:  // Insufficient Authentication
        return GATT_ERROR_NOT_AUTHORIZED;
      case 0x06:  // Request Not Supported
        return GATT_ERROR_NOT_SUPPORTED;
      case 0x07:  // Invalid Offset
        return GATT_ERROR_INVALID_LENGTH;
      case 0x08:  // Insufficient Authorization
        return GATT_ERROR_NOT_AUTHORIZED;
      case 0x09:  // Prepare Queue Full
        return GATT_ERROR_IN_PROGRESS;
      case 0x0A:  // Attribute Not Found
        return GATT_ERROR_FAILED;
      case 0x0B:  // Attribute Not Long
        return GATT_ERROR_FAILED;
      case 0x0C:  // Insufficient Encryption Key Size
        return GATT_ERROR_FAILED;
      case 0x0D:  // Invalid Attribute Value Length
        return GATT_ERROR_INVALID_LENGTH;
      case 0x0E:  // Unlikely Error
        return GATT_ERROR_FAILED;
      case 0x0F:  // Insufficient Encryption
        return GATT_ERROR_NOT_PAIRED;
      case 0x10:  // Unsupported Group Type
        return GATT_ERROR_FAILED;
      case 0x11:  // Insufficient Resources
        return GATT_ERROR_FAILED;
      default:
        return GATT_ERROR_UNKNOWN;
    }
  }

  static uint8_t ToProtocolError(GattErrorCode error_code);

 private:
  BluetoothRemoteGattServiceWinrt(
      BluetoothDevice* device,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattDeviceService>
          gatt_service,
      BluetoothUUID uuid,
      uint16_t attribute_handle);

  BluetoothDevice* device_;
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                             GenericAttributeProfile::IGattDeviceService>
      gatt_service_;
  BluetoothUUID uuid_;
  uint16_t attribute_handle_;
  std::string identifier_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattServiceWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WINRT_H_
