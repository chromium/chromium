// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CHARACTERISTIC_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CHARACTERISTIC_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class BluetoothRemoteGattDescriptor;
class BluetoothRemoteGattService;
class MockBluetoothGattDescriptor;
class MockBluetoothGattService;

class MockBluetoothGattCharacteristic
    : public BluetoothRemoteGattCharacteristic {
 public:
  MockBluetoothGattCharacteristic(MockBluetoothGattService* service,
                                  const std::string& identifier,
                                  const BluetoothUUID& uuid,
                                  Properties properties,
                                  Permissions permissions);

  MockBluetoothGattCharacteristic(const MockBluetoothGattCharacteristic&) =
      delete;
  MockBluetoothGattCharacteristic& operator=(
      const MockBluetoothGattCharacteristic&) = delete;

  ~MockBluetoothGattCharacteristic() override;

  MOCK_CONST_METHOD0(GetIdentifier, std::string());
  MOCK_CONST_METHOD0(GetUUID, BluetoothUUID());
  MOCK_CONST_METHOD0(GetValue, const std::vector<uint8_t>&());
  MOCK_CONST_METHOD0(GetService, BluetoothRemoteGattService*());
  MOCK_CONST_METHOD0(GetProperties, Properties());
  MOCK_CONST_METHOD0(GetPermissions, Permissions());
  MOCK_CONST_METHOD0(IsNotifying, bool());
  MOCK_CONST_METHOD0(GetDescriptors,
                     std::vector<BluetoothRemoteGattDescriptor*>());
  MOCK_CONST_METHOD1(GetDescriptor,
                     BluetoothRemoteGattDescriptor*(const std::string&));
#if BUILDFLAG(IS_CHROMEOS)
  void StartNotifySession(NotificationType t,
                          NotifySessionCallback c,
                          ErrorCallback ec) override {
    StartNotifySession_(t, c, ec);
  }
  MOCK_METHOD3(StartNotifySession_,
               void(NotificationType, NotifySessionCallback&, ErrorCallback&));
#endif  // BUILDFLAG(IS_CHROMEOS)
  void StartNotifySession(NotifySessionCallback c, ErrorCallback ec) override {
    StartNotifySession_(c, ec);
  }
  MOCK_METHOD2(StartNotifySession_,
               void(NotifySessionCallback&, ErrorCallback&));
  void StopNotifySession(BluetoothGattNotifySession::Id s,
                         base::OnceClosure c) override {
    StopNotifySession_(s, c);
  }
  MOCK_METHOD2(StopNotifySession_,
               void(BluetoothGattNotifySession::Id, base::OnceClosure&));
  void ReadRemoteCharacteristic(ValueCallback c) override {
    ReadRemoteCharacteristic_(c);
  }
  MOCK_METHOD1(ReadRemoteCharacteristic_, void(ValueCallback&));
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& v,
                                 WriteType t,
                                 base::OnceClosure c,
                                 ErrorCallback ec) override {
    WriteRemoteCharacteristic_(v, t, c, ec);
  }
  MOCK_METHOD4(WriteRemoteCharacteristic_,
               void(const std::vector<uint8_t>&,
                    WriteType,
                    base::OnceClosure&,
                    ErrorCallback&));
  void DeprecatedWriteRemoteCharacteristic(const std::vector<uint8_t>& v,
                                           base::OnceClosure c,
                                           ErrorCallback ec) override {
    DeprecatedWriteRemoteCharacteristic_(v, c, ec);
  }
  MOCK_METHOD3(DeprecatedWriteRemoteCharacteristic_,
               void(const std::vector<uint8_t>&,
                    base::OnceClosure&,
                    ErrorCallback&));
#if BUILDFLAG(IS_CHROMEOS)
  void PrepareWriteRemoteCharacteristic(const std::vector<uint8_t>& v,
                                        base::OnceClosure c,
                                        ErrorCallback ec) override {
    PrepareWriteRemoteCharacteristic_(v, c, ec);
  }
  MOCK_METHOD3(PrepareWriteRemoteCharacteristic_,
               void(const std::vector<uint8_t>&,
                    base::OnceClosure&,
                    ErrorCallback&));
#endif  // BUILDFLAG(IS_CHROMEOS)

  void AddMockDescriptor(
      std::unique_ptr<MockBluetoothGattDescriptor> mock_descriptor);

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  void SubscribeToNotifications(BluetoothRemoteGattDescriptor* d,
                                NotificationType t,
                                base::OnceClosure c,
                                ErrorCallback ec) override {
    SubscribeToNotifications_(d, t, c, ec);
  }
  MOCK_METHOD4(SubscribeToNotifications_,
               void(BluetoothRemoteGattDescriptor*,
                    NotificationType,
                    base::OnceClosure&,
                    ErrorCallback&));
#else
  void SubscribeToNotifications(BluetoothRemoteGattDescriptor* d,
                                base::OnceClosure c,
                                ErrorCallback ec) override {
    SubscribeToNotifications_(d, c, ec);
  }
  MOCK_METHOD3(SubscribeToNotifications_,
               void(BluetoothRemoteGattDescriptor*,
                    base::OnceClosure&,
                    ErrorCallback&));
#endif  // BUILDFLAG(IS_CHROMEOS)
  void UnsubscribeFromNotifications(BluetoothRemoteGattDescriptor* d,
                                    base::OnceClosure c,
                                    ErrorCallback ec) override {
    UnsubscribeFromNotifications_(d, c, ec);
  }
  MOCK_METHOD3(UnsubscribeFromNotifications_,
               void(BluetoothRemoteGattDescriptor*,
                    base::OnceClosure&,
                    ErrorCallback&));
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CHARACTERISTIC_H_
