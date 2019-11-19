// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_device.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"

namespace device {

MockBluetoothDevice::MockBluetoothDevice(MockBluetoothAdapter* adapter,
                                         uint32_t bluetooth_class,
                                         const char* name,
                                         const std::string& address,
                                         bool paired,
                                         bool connected)
    : BluetoothDevice(adapter),
      bluetooth_class_(bluetooth_class),
      name_(name ? base::Optional<std::string>(name) : base::nullopt),
      address_(address),
      connected_(connected) {
  ON_CALL(*this, GetBluetoothClass())
      .WillByDefault(testing::Return(bluetooth_class_));
  ON_CALL(*this, GetIdentifier())
      .WillByDefault(testing::Return(address_ + "-Identifier"));
  ON_CALL(*this, GetAddress())
      .WillByDefault(testing::Return(address_));
  ON_CALL(*this, GetVendorIDSource())
      .WillByDefault(testing::Return(VENDOR_ID_UNKNOWN));
  ON_CALL(*this, GetVendorID())
      .WillByDefault(testing::Return(0));
  ON_CALL(*this, GetProductID())
      .WillByDefault(testing::Return(0));
  ON_CALL(*this, GetDeviceID())
      .WillByDefault(testing::Return(0));
  ON_CALL(*this, GetName()).WillByDefault(testing::Return(name_));
  ON_CALL(*this, GetNameForDisplay())
      .WillByDefault(testing::Return(
          base::UTF8ToUTF16(name_ ? name_.value() : "Unnamed Device")));
  ON_CALL(*this, GetDeviceType())
      .WillByDefault(testing::Return(BluetoothDeviceType::UNKNOWN));
  ON_CALL(*this, IsPaired())
      .WillByDefault(testing::Return(paired));
  ON_CALL(*this, IsConnected())
      .WillByDefault(testing::ReturnPointee(&connected_));
  ON_CALL(*this, IsGattConnected())
      .WillByDefault(testing::ReturnPointee(&connected_));
  ON_CALL(*this, IsConnectable())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, IsConnecting())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, GetUUIDs()).WillByDefault(testing::ReturnPointee(&uuids_));
  ON_CALL(*this, ExpectingPinCode())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, ExpectingPasskey())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, ExpectingConfirmation())
      .WillByDefault(testing::Return(false));
}

MockBluetoothDevice::~MockBluetoothDevice() = default;

void MockBluetoothDevice::AddMockService(
    std::unique_ptr<MockBluetoothGattService> mock_service) {
  mock_services_.push_back(std::move(mock_service));
}

std::vector<BluetoothRemoteGattService*> MockBluetoothDevice::GetMockServices()
    const {
  std::vector<BluetoothRemoteGattService*> services;
  for (const auto& service : mock_services_) {
    services.push_back(service.get());
  }
  return services;
}

BluetoothRemoteGattService* MockBluetoothDevice::GetMockService(
    const std::string& identifier) const {
  for (const auto& service : mock_services_) {
    if (service->GetIdentifier() == identifier)
      return service.get();
  }
  return nullptr;
}

void MockBluetoothDevice::PushPendingCallback(base::OnceClosure callback) {
  pending_callbacks_.push(std::move(callback));
}

void MockBluetoothDevice::RunPendingCallbacks() {
  while (!pending_callbacks_.empty()) {
    std::move(pending_callbacks_.front()).Run();
    pending_callbacks_.pop();
  }
}

}  // namespace device
