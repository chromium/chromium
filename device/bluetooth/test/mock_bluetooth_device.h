// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class BluetoothAdapter;
class BluetoothRemoteGattService;

class MockBluetoothDevice : public BluetoothDevice {
 public:
  MockBluetoothDevice(BluetoothAdapter* adapter,
                      uint32_t bluetooth_class,
                      const char* name,
                      const std::string& address,
                      bool initially_paired,
                      bool connected);
  ~MockBluetoothDevice() override;

  MOCK_CONST_METHOD0(GetBluetoothClass, uint32_t());
  MOCK_CONST_METHOD0(GetType, BluetoothTransport());
  MOCK_CONST_METHOD0(GetIdentifier, std::string());
  MOCK_CONST_METHOD0(GetAddress, std::string());
  MOCK_CONST_METHOD0(GetAddressType, BluetoothDevice::AddressType());
  MOCK_CONST_METHOD0(GetVendorIDSource, BluetoothDevice::VendorIDSource());
  MOCK_CONST_METHOD0(GetVendorID, uint16_t());
  MOCK_CONST_METHOD0(GetProductID, uint16_t());
  MOCK_CONST_METHOD0(GetDeviceID, uint16_t());
  MOCK_CONST_METHOD0(GetAppearance, uint16_t());
  MOCK_CONST_METHOD0(GetName, std::optional<std::string>());
  MOCK_CONST_METHOD0(GetNameForDisplay, std::u16string());
  MOCK_CONST_METHOD0(GetDeviceType, BluetoothDeviceType());
  MOCK_CONST_METHOD0(IsPaired, bool());
#if BUILDFLAG(IS_CHROMEOS)
  MOCK_CONST_METHOD0(IsBonded, bool());
#endif  // BUILDFLAG(IS_CHROMEOS)
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsGattConnected, bool());
  MOCK_CONST_METHOD0(IsConnectable, bool());
  MOCK_CONST_METHOD0(IsConnecting, bool());
  MOCK_CONST_METHOD0(GetUUIDs, UUIDSet());
  MOCK_CONST_METHOD0(GetInquiryRSSI, std::optional<int8_t>());
  MOCK_CONST_METHOD0(GetInquiryTxPower, std::optional<int8_t>());
  MOCK_CONST_METHOD0(ExpectingPinCode, bool());
  MOCK_CONST_METHOD0(ExpectingPasskey, bool());
  MOCK_CONST_METHOD0(ExpectingConfirmation, bool());
  MOCK_METHOD1(GetConnectionInfo, void(ConnectionInfoCallback callback));
  MOCK_METHOD3(SetConnectionLatency,
               void(ConnectionLatency connection_latency,
                    base::OnceClosure callback,
                    ErrorCallback error_callback));
  MOCK_METHOD2(Connect,
               void(BluetoothDevice::PairingDelegate* pairing_delegate,
                    ConnectCallback callback));
#if BUILDFLAG(IS_CHROMEOS)
  MOCK_METHOD2(ConnectClassic,
               void(BluetoothDevice::PairingDelegate* pairing_delegate,
                    ConnectCallback callback));
#endif  // BUILDFLAG(IS_CHROMEOS)
  MOCK_METHOD2(Pair,
               void(BluetoothDevice::PairingDelegate* pairing_delegate,
                    ConnectCallback callback));
  MOCK_METHOD1(SetPinCode, void(const std::string&));
  MOCK_METHOD1(SetPasskey, void(uint32_t));
  MOCK_METHOD0(ConfirmPairing, void());
  MOCK_METHOD0(RejectPairing, void());
  MOCK_METHOD0(CancelPairing, void());
  MOCK_METHOD2(Disconnect,
               void(base::OnceClosure callback,
                    BluetoothDevice::ErrorCallback error_callback));
  MOCK_METHOD2(Forget,
               void(base::OnceClosure callback,
                    BluetoothDevice::ErrorCallback error_callback));
  MOCK_METHOD3(ConnectToService,
               void(const BluetoothUUID& uuid,
                    ConnectToServiceCallback callback,
                    ConnectToServiceErrorCallback error_callback));
  MOCK_METHOD3(ConnectToServiceInsecurely,
               void(const BluetoothUUID& uuid,
                    ConnectToServiceCallback callback,
                    ConnectToServiceErrorCallback error_callback));
  MOCK_METHOD2(CreateGattConnection,
               void(GattConnectionCallback callback,
                    std::optional<BluetoothUUID> service_uuid));
  MOCK_CONST_METHOD0(IsGattServicesDiscoveryComplete, bool());

  MOCK_CONST_METHOD0(GetGattServices,
                     std::vector<BluetoothRemoteGattService*>());
  MOCK_CONST_METHOD1(GetGattService,
                     BluetoothRemoteGattService*(const std::string&));
#if BUILDFLAG(IS_APPLE)
  MOCK_METHOD0(IsLowEnergyDevice, bool());
#endif  // BUILDFLAG(IS_APPLE)
  MOCK_METHOD1(CreateGattConnectionImpl,
               void(std::optional<BluetoothUUID> service_uuid));
  MOCK_METHOD0(DisconnectGatt, void());
#if BUILDFLAG(IS_CHROMEOS)
  MOCK_METHOD2(ExecuteWrite,
               void(base::OnceClosure callback,
                    ExecuteWriteErrorCallback error_callback));
  MOCK_METHOD2(AbortWrite,
               void(base::OnceClosure callback,
                    AbortWriteErrorCallback error_callback));
#endif  // BUILDFLAG(IS_CHROMEOS)

  // BluetoothDevice manages the lifetime of its BluetoothGATTServices.
  // This method takes ownership of the MockBluetoothGATTServices. This is only
  // for convenience as far as testing is concerned, and it's possible to write
  // test cases without using these functions.
  // Example:
  // ON_CALL(*mock_device, GetGattServices))
  //   .WillByDefault(Invoke(*mock_device,
  //                         &MockBluetoothDevice::GetMockServices));
  void AddMockService(std::unique_ptr<MockBluetoothGattService> mock_service);
  std::vector<BluetoothRemoteGattService*> GetMockServices() const;
  BluetoothRemoteGattService* GetMockService(
      const std::string& identifier) const;

  void AddUUID(const BluetoothUUID& uuid) { uuids_.insert(uuid); }

  void SetServiceDataForUUID(const BluetoothUUID& uuid,
                             const std::vector<uint8_t>& service_data) {
    service_data_[uuid] = service_data;
  }

  // Updates the device's Manufacturer Data that are returned by
  // BluetoothDevice::GetManufacturerData().
  void SetManufacturerData(ManufacturerDataMap manufacturer_data) {
    manufacturer_data_ = std::move(manufacturer_data);
  }

  // Functions to save and run callbacks from this device. Useful when
  // trying to run callbacks in response to other actions e.g. run a read
  // value callback in response to a connection request.
  // Appends callback to the end of the callbacks queue.
  void PushPendingCallback(base::OnceClosure callback);
  // Runs all pending callbacks.
  void RunPendingCallbacks();

  void SetConnected(bool connected) { connected_ = connected; }

  void SetPaired(bool paired) { paired_ = paired; }

  void SetType(device::BluetoothTransport transport) { transport_ = transport; }

 private:
  uint32_t bluetooth_class_;
  std::optional<std::string> name_;
  std::string address_;
  BluetoothDevice::UUIDSet uuids_;
  bool connected_;
  bool paired_;
  device::BluetoothTransport transport_ =
      device::BluetoothTransport::BLUETOOTH_TRANSPORT_INVALID;

  // Used by tests to save callbacks that will be run in the future.
  base::queue<base::OnceClosure> pending_callbacks_;

  std::vector<std::unique_ptr<MockBluetoothGattService>> mock_services_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
