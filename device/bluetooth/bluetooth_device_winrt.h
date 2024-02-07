// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WINRT_H_

#include <stdint.h>
#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <windows.devices.bluetooth.h>
#include <wrl/client.h>

#include <memory>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/win/windows_version.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class BluetoothAdapterWinrt;
class BluetoothGattDiscovererWinrt;
class BluetoothPairingWinrt;
class BluetoothUUID;

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

  BluetoothDeviceWinrt(const BluetoothDeviceWinrt&) = delete;
  BluetoothDeviceWinrt& operator=(const BluetoothDeviceWinrt&) = delete;

  ~BluetoothDeviceWinrt() override;

  // BluetoothDevice:
  uint32_t GetBluetoothClass() const override;
  std::string GetAddress() const override;
  AddressType GetAddressType() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  std::optional<std::string> GetName() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(ConnectionInfoCallback callback) override;
  void SetConnectionLatency(ConnectionLatency connection_latency,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) override;
  void Connect(PairingDelegate* pairing_delegate,
               ConnectCallback callback) override;
  void Pair(PairingDelegate* pairing_delegate,
            ConnectCallback callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void Forget(base::OnceClosure callback,
              ErrorCallback error_callback) override;
  void ConnectToService(const BluetoothUUID& uuid,
                        ConnectToServiceCallback callback,
                        ConnectToServiceErrorCallback error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      ConnectToServiceCallback callback,
      ConnectToServiceErrorCallback error_callback) override;

  // Returns the |address| in the canonical format: XX:XX:XX:XX:XX:XX, where
  // each 'X' is a hex digit.
  static std::string CanonicalizeAddress(uint64_t address);

  // Called by BluetoothAdapterWinrt when an advertisement packet is received.
  void UpdateLocalName(std::optional<std::string> local_name);

 protected:
  // BluetoothDevice:
  void CreateGattConnectionImpl(
      std::optional<BluetoothUUID> service_uuid) override;
  void UpgradeToFullDiscovery() override;
  void DisconnectGatt() override;

  // Declared virtual so that it can be overridden by tests.
  virtual HRESULT GetBluetoothLEDeviceStaticsActivationFactory(
      ABI::Windows::Devices::Bluetooth::IBluetoothLEDeviceStatics** statics)
      const;

  // Declared virtual so that it can be overridden by tests.
  virtual HRESULT GetGattSessionStaticsActivationFactory(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          IGattSessionStatics** statics) const;

  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice>
      ble_device_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::IGattSession>
      gatt_session_;

 private:
  void OnBluetoothLEDeviceFromBluetoothAddress(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice> ble_device);

  void OnGattSessionFromDeviceId(
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattSession>
          gatt_session);

  void OnGattSessionStatusChanged(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::IGattSession*
          gatt_session,
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          IGattSessionStatusChangedEventArgs* event_args);

  void OnConnectionStatusChanged(
      ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice* ble_device,
      IInspectable* object);

  void OnGattServicesChanged(
      ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice* ble_device,
      IInspectable* object);

  void OnNameChanged(
      ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice* ble_device,
      IInspectable* object);

  void StartGattDiscovery();
  void OnGattDiscoveryComplete(bool success);
  void NotifyGattConnectFailure();

  void ClearGattServices();
  void ClearEventRegistrations();

  ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus
      connection_status_;
  ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattSessionStatus
      gatt_session_status_;
  uint64_t raw_address_;
  std::string address_;
  std::optional<std::string> local_name_;

  std::unique_ptr<BluetoothPairingWinrt> pairing_;

  // Indicates whether the device should subscribe to GattSession
  // SessionStatusChanged events. Doing so requires calling
  // BluetoothLEDevice::GetDeviceId() which is only available on 1709
  // (RS3) or newer. If false, GATT connection reliability may be
  // degraded.
  bool observe_gatt_session_status_change_events_ =
      base::FeatureList::IsEnabled(kNewBLEGattSessionHandling) &&
      base::win::GetVersion() >= base::win::Version::WIN10_RS3;

  // Indicates whether a GATT service discovery is imminent. Discovery
  // begins once GattSessionStatus for the device changes to |Active|
  // if |observe_gatt_session_status_change_events_| is true, or once
  // the BluetoothLEDevice has been obtained from
  // FromBluetoothAddressAsync() otherwise.
  bool pending_gatt_service_discovery_start_ = false;

  std::optional<BluetoothUUID> target_uuid_;
  std::unique_ptr<BluetoothGattDiscovererWinrt> gatt_discoverer_;

  std::optional<EventRegistrationToken> connection_changed_token_;
  std::optional<EventRegistrationToken> gatt_session_status_changed_token_;
  std::optional<EventRegistrationToken> gatt_services_changed_token_;
  std::optional<EventRegistrationToken> name_changed_token_;

  THREAD_CHECKER(thread_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDeviceWinrt> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WINRT_H_
