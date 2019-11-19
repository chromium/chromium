// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class BluetoothRemoteGattService;
class MockBluetoothAdapter;

class MockBluetoothDevice : public BluetoothDevice {
 public:
  MockBluetoothDevice(MockBluetoothAdapter* adapter,
                      uint32_t bluetooth_class,
                      const char* name,
                      const std::string& address,
                      bool paired,
                      bool connected);
  ~MockBluetoothDevice() override;

  MOCK_CONST_METHOD0(GetBluetoothClass, uint32_t());
  MOCK_CONST_METHOD0(GetType, BluetoothTransport());
  MOCK_CONST_METHOD0(GetIdentifier, std::string());
  MOCK_CONST_METHOD0(GetAddress, std::string());
  MOCK_CONST_METHOD0(GetVendorIDSource, BluetoothDevice::VendorIDSource());
  MOCK_CONST_METHOD0(GetVendorID, uint16_t());
  MOCK_CONST_METHOD0(GetProductID, uint16_t());
  MOCK_CONST_METHOD0(GetDeviceID, uint16_t());
  MOCK_CONST_METHOD0(GetAppearance, uint16_t());
  MOCK_CONST_METHOD0(GetName, base::Optional<std::string>());
  MOCK_CONST_METHOD0(GetNameForDisplay, base::string16());
  MOCK_CONST_METHOD0(GetDeviceType, BluetoothDeviceType());
  MOCK_CONST_METHOD0(IsPaired, bool());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsGattConnected, bool());
  MOCK_CONST_METHOD0(IsConnectable, bool());
  MOCK_CONST_METHOD0(IsConnecting, bool());
  MOCK_CONST_METHOD0(GetUUIDs, UUIDSet());
  MOCK_CONST_METHOD0(GetInquiryRSSI, base::Optional<int8_t>());
  MOCK_CONST_METHOD0(GetInquiryTxPower, base::Optional<int8_t>());
  MOCK_CONST_METHOD0(ExpectingPinCode, bool());
  MOCK_CONST_METHOD0(ExpectingPasskey, bool());
  MOCK_CONST_METHOD0(ExpectingConfirmation, bool());
  MOCK_METHOD1(GetConnectionInfo, void(const ConnectionInfoCallback& callback));
  MOCK_METHOD3(SetConnectionLatency,
               void(ConnectionLatency connection_latency,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(Connect,
               void(BluetoothDevice::PairingDelegate* pairing_delegate,
                    const base::Closure& callback,
                    const BluetoothDevice::ConnectErrorCallback&
                        error_callback));
  MOCK_METHOD3(
      Pair,
      void(BluetoothDevice::PairingDelegate* pairing_delegate,
           const base::Closure& callback,
           const BluetoothDevice::ConnectErrorCallback& error_callback));
  MOCK_METHOD1(SetPinCode, void(const std::string&));
  MOCK_METHOD1(SetPasskey, void(uint32_t));
  MOCK_METHOD0(ConfirmPairing, void());
  MOCK_METHOD0(RejectPairing, void());
  MOCK_METHOD0(CancelPairing, void());
  MOCK_METHOD2(Disconnect,
               void(const base::Closure& callback,
                    const BluetoothDevice::ErrorCallback& error_callback));
  MOCK_METHOD2(Forget,
               void(const base::Closure& callback,
                    const BluetoothDevice::ErrorCallback& error_callback));
  MOCK_METHOD3(ConnectToService,
               void(const BluetoothUUID& uuid,
                    const ConnectToServiceCallback& callback,
                    const ConnectToServiceErrorCallback& error_callback));
  MOCK_METHOD3(ConnectToServiceInsecurely,
               void(const BluetoothUUID& uuid,
                    const ConnectToServiceCallback& callback,
                    const ConnectToServiceErrorCallback& error_callback));
  MOCK_METHOD2(CreateGattConnection,
               void(const GattConnectionCallback& callback,
                    const ConnectErrorCallback& error_callback));

  MOCK_METHOD1(SetGattServicesDiscoveryComplete, void(bool));
  MOCK_CONST_METHOD0(IsGattServicesDiscoveryComplete, bool());

  MOCK_CONST_METHOD0(GetGattServices,
                     std::vector<BluetoothRemoteGattService*>());
  MOCK_CONST_METHOD1(GetGattService,
                     BluetoothRemoteGattService*(const std::string&));
  MOCK_METHOD0(CreateGattConnectionImpl, void());
  MOCK_METHOD0(DisconnectGatt, void());
#if defined(OS_CHROMEOS)
  MOCK_METHOD2(ExecuteWrite,
               void(const base::Closure& callback,
                    const ExecuteWriteErrorCallback& error_callback));
  MOCK_METHOD2(AbortWrite,
               void(const base::Closure& callback,
                    const AbortWriteErrorCallback& error_callback));
#endif

  // BluetoothDevice manages the lifetime of its BluetoothGATTServices.
  // This method takes ownership of the MockBluetoothGATTServices. This is only
  // for convenience as far as testing is concerned, and it's possible to write
  // test cases without using these functions.
  // Example:
  // ON_CALL(*mock_device, GetGattServices))
  //   .WillByDefault(Invoke(*mock_device,
  //                         &MockBluetoothDevice::GetMockServices));
  void AddMockService(std::unique_ptr<MockBluetoothGattService> mock_device);
  std::vector<BluetoothRemoteGattService*> GetMockServices() const;
  BluetoothRemoteGattService* GetMockService(
      const std::string& identifier) const;

  void AddUUID(const BluetoothUUID& uuid) { uuids_.insert(uuid); }

  // Functions to save and run callbacks from this device. Useful when
  // trying to run callbacks in response to other actions e.g. run a read
  // value callback in response to a connection request.
  // Appends callback to the end of the callbacks queue.
  void PushPendingCallback(base::OnceClosure callback);
  // Runs all pending callbacks.
  void RunPendingCallbacks();

  void SetConnected(bool connected) { connected_ = connected; }

 private:
  uint32_t bluetooth_class_;
  base::Optional<std::string> name_;
  std::string address_;
  BluetoothDevice::UUIDSet uuids_;
  bool connected_;

  // Used by tests to save callbacks that will be run in the future.
  base::queue<base::OnceClosure> pending_callbacks_;

  std::vector<std::unique_ptr<MockBluetoothGattService>> mock_services_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
