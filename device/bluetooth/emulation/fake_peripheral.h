// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_EMULATION_FAKE_PERIPHERAL_H_
#define DEVICE_BLUETOOTH_EMULATION_FAKE_PERIPHERAL_H_

#include <optional>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/emulation/fake_central.h"
#include "device/bluetooth/emulation/fake_remote_gatt_service.h"

namespace device {
class BluetoothUUID;
}

namespace bluetooth {

// Implements device::BluetoothDevice. Meant to be used by FakeCentral
// to keep track of the peripheral's state and attributes.
//
// Not intended for direct use by clients.  See README.md.
class FakePeripheral : public device::BluetoothDevice {
 public:
  FakePeripheral(FakeCentral* fake_central, const std::string& address);

  FakePeripheral(const FakePeripheral&) = delete;
  FakePeripheral& operator=(const FakePeripheral&) = delete;

  ~FakePeripheral() override;

  // Changes the name of the device.
  void SetName(std::optional<std::string> name);

  // Set it to indicate if the system has connected to the Peripheral outside of
  // the Bluetooth interface e.g. the user connected to the device through
  // system settings.
  void SetSystemConnected(bool gatt_connected);

  // Updates the peripheral's UUIDs that are returned by
  // BluetoothDevice::GetUUIDs().
  void SetServiceUUIDs(UUIDSet service_uuids);

  // Updates the peripheral's Manufacturer Data that are returned by
  // BluetoothDevice::GetManufacturerData().
  void SetManufacturerData(ManufacturerDataMap manufacturer_data);

  // If |code| is kHCISuccess calls a pending success callback for
  // CreateGattConnection. Otherwise calls a pending error callback
  // with the ConnectErrorCode corresponding to |code|.
  void SetNextGATTConnectionResponse(uint16_t code);

  // If |code| is kHCISuccess, calls
  // BluetoothAdapter::Observer::GattServicesDiscovered otherwise
  // sets IsGattDiscoveryComplete to false. Both of these happen
  // after IsGattDiscoveryComplete is called.
  void SetNextGATTDiscoveryResponse(uint16_t code);

  // Returns true if there are no pending responses for this peripheral or any
  // of its GATT services.
  bool AllResponsesConsumed();

  // Simulates a GATT disconnection from the peripheral.
  void SimulateGATTDisconnection();

  // Adds a fake primary service with |service_uuid| to this peripheral.
  // Returns the service's Id.
  std::string AddFakeService(const device::BluetoothUUID& service_uuid);

  // Remove a fake service with |identifier| from this peripheral.
  bool RemoveFakeService(const std::string& identifier);

  // BluetoothDevice overrides:
  uint32_t GetBluetoothClass() const override;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  device::BluetoothTransport GetType() const override;
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  std::string GetIdentifier() const override;
  std::string GetAddress() const override;
  AddressType GetAddressType() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  std::optional<std::string> GetName() const override;
  std::u16string GetNameForDisplay() const override;
  bool IsPaired() const override;
#if BUILDFLAG(IS_CHROMEOS)
  bool IsBonded() const override;
#endif  // BUILDFLAG(IS_CHROMEOS)
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
#if BUILDFLAG(IS_CHROMEOS)
  void ConnectClassic(PairingDelegate* pairing_delegate,
                      ConnectCallback callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void Forget(base::OnceClosure callback,
              ErrorCallback error_callback) override;
  void ConnectToService(const device::BluetoothUUID& uuid,
                        ConnectToServiceCallback callback,
                        ConnectToServiceErrorCallback error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      ConnectToServiceCallback callback,
      ConnectToServiceErrorCallback error_callback) override;
  void CreateGattConnection(
      GattConnectionCallback callback,
      std::optional<device::BluetoothUUID> service_uuid) override;
  bool IsGattServicesDiscoveryComplete() const override;
#if BUILDFLAG(IS_APPLE)
  bool IsLowEnergyDevice() override;
#endif  // BUILDFLAG(IS_APPLE)
#if BUILDFLAG(IS_CHROMEOS)
  void ExecuteWrite(base::OnceClosure callback,
                    ExecuteWriteErrorCallback error_callback) override;
  void AbortWrite(base::OnceClosure callback,
                  AbortWriteErrorCallback error_callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

 protected:
  void CreateGattConnectionImpl(std::optional<device::BluetoothUUID>) override;
  void DisconnectGatt() override;

 private:
  void DispatchConnectionResponse();
  void DispatchDiscoveryResponse();

  const std::string address_;
  std::optional<std::string> name_;
  // True when the system has connected to the device outside of the Bluetooth
  // interface e.g. the user connected to the device through system settings.
  bool system_connected_;
  // True when this Bluetooth interface is connected to the device.
  bool gatt_connected_;

  // Keeps track of the last Id used to create a fake service. Incremented
  // every time a new fake service is added.
  size_t last_service_id_;

  // Used to simulate a GATT Discovery procedure.
  // Mutable because IsGattServicesDiscoveryComplete needs to set this but
  // is const.
  mutable bool pending_gatt_discovery_;

  // Used to decide which callback should be called when
  // CreateGattConnection is called.
  std::optional<uint16_t> next_connection_response_;

  // Used to decide if the GattServicesDiscovered method is called.
  std::optional<uint16_t> next_discovery_response_;

  // Mutable because IsGattServicesDiscoveryComplete needs to post a task but
  // is const.
  mutable base::WeakPtrFactory<FakePeripheral> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_EMULATION_FAKE_PERIPHERAL_H_
