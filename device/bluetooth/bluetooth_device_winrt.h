// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WINRT_H_

#include <windows.devices.bluetooth.h>
#include <wrl/client.h>

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class BluetoothAdapterWinrt;
class BluetoothGattDiscovererWinrt;
class BluetoothPairingWinrt;

class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceWinrt : public BluetoothDevice {
 public:
  // Constants required to extract the tx power level and service data from the
  // raw advertisementment data. Reference:
  // https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile
  static constexpr uint8_t kTxPowerLevelDataSection = 0x0A;
  static constexpr uint8_t k16BitServiceDataSection = 0x16;
  static constexpr uint8_t k32BitServiceDataSection = 0x20;
  static constexpr uint8_t k128BitServiceDataSection = 0x21;

  BluetoothDeviceWinrt(BluetoothAdapterWinrt* adapter, uint64_t raw_address);
  ~BluetoothDeviceWinrt() override;

  // BluetoothDevice:
  uint32_t GetBluetoothClass() const override;
  std::string GetAddress() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  base::Optional<std::string> GetName() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(const ConnectionInfoCallback& callback) override;
  void SetConnectionLatency(ConnectionLatency connection_latency,
                            const base::Closure& callback,
                            const ErrorCallback& error_callback) override;
  void Connect(PairingDelegate* pairing_delegate,
               const base::Closure& callback,
               const ConnectErrorCallback& error_callback) override;
  void Pair(PairingDelegate* pairing_delegate,
            const base::Closure& callback,
            const ConnectErrorCallback& error_callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void Forget(const base::Closure& callback,
              const ErrorCallback& error_callback) override;
  void ConnectToService(
      const BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;

  // Returns the |address| in the canonical format: XX:XX:XX:XX:XX:XX, where
  // each 'X' is a hex digit.
  static std::string CanonicalizeAddress(uint64_t address);

  // Called by BluetoothAdapterWinrt when an advertisement packet is received.
  void UpdateLocalName(base::Optional<std::string> local_name);

 protected:
  // BluetoothDevice:
  void CreateGattConnectionImpl() override;
  void DisconnectGatt() override;

  // This is declared virtual so that they can be overridden by tests.
  virtual HRESULT GetBluetoothLEDeviceStaticsActivationFactory(
      ABI::Windows::Devices::Bluetooth::IBluetoothLEDeviceStatics** statics)
      const;

  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice>
      ble_device_;

 private:
  void OnFromBluetoothAddress(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice> ble_device);

  void OnConnectionStatusChanged(
      ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice* ble_device,
      IInspectable* object);

  void OnGattServicesChanged(
      ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice* ble_device,
      IInspectable* object);

  void OnNameChanged(
      ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice* ble_device,
      IInspectable* object);

  void OnGattDiscoveryComplete(bool success);

  void ClearGattServices();
  void ClearEventRegistrations();

  uint64_t raw_address_;
  std::string address_;
  base::Optional<std::string> local_name_;

  std::unique_ptr<BluetoothPairingWinrt> pairing_;

  std::unique_ptr<BluetoothGattDiscovererWinrt> gatt_discoverer_;

  base::Optional<EventRegistrationToken> connection_changed_token_;
  base::Optional<EventRegistrationToken> gatt_services_changed_token_;
  base::Optional<EventRegistrationToken> name_changed_token_;

  THREAD_CHECKER(thread_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDeviceWinrt> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WINRT_H_
