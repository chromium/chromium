// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_adapter.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"

namespace device {

using testing::Invoke;
using testing::_;

MockBluetoothAdapter::Observer::Observer(
    scoped_refptr<BluetoothAdapter> adapter)
    : adapter_(std::move(adapter)) {
  adapter_->AddObserver(this);
}

MockBluetoothAdapter::Observer::~Observer() {
  adapter_->RemoveObserver(this);
}

MockBluetoothAdapter::MockBluetoothAdapter() {
  ON_CALL(*this, AddObserver(_))
      .WillByDefault(Invoke([this](BluetoothAdapter::Observer* observer) {
        this->BluetoothAdapter::AddObserver(observer);
      }));
  ON_CALL(*this, RemoveObserver(_))
      .WillByDefault(Invoke([this](BluetoothAdapter::Observer* observer) {
        this->BluetoothAdapter::RemoveObserver(observer);
      }));
}

MockBluetoothAdapter::~MockBluetoothAdapter() = default;

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
void MockBluetoothAdapter::Shutdown() {
}
#endif

base::WeakPtr<BluetoothAdapter> MockBluetoothAdapter::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool MockBluetoothAdapter::SetPoweredImpl(bool powered) {
  return false;
}

void MockBluetoothAdapter::StartScanWithFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  StartScanWithFilter_(discovery_filter.get(), callback);
}

void MockBluetoothAdapter::UpdateFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  UpdateFilter_(discovery_filter.get(), callback);
}

void MockBluetoothAdapter::AddMockDevice(
    std::unique_ptr<MockBluetoothDevice> mock_device) {
  mock_devices_.push_back(std::move(mock_device));
}

std::unique_ptr<MockBluetoothDevice> MockBluetoothAdapter::RemoveMockDevice(
    const std::string& address) {
  for (auto it = mock_devices_.begin(); it != mock_devices_.end(); ++it) {
    if ((*it)->GetAddress() != address) {
      continue;
    }
    std::unique_ptr<MockBluetoothDevice> removed_device = std::move(*it);
    mock_devices_.erase(it);
    return removed_device;
  }
  return nullptr;
}

BluetoothAdapter::ConstDeviceList MockBluetoothAdapter::GetConstMockDevices() {
  BluetoothAdapter::ConstDeviceList devices;
  for (const auto& it : mock_devices_) {
    devices.push_back(it.get());
  }
  return devices;
}

BluetoothAdapter::DeviceList MockBluetoothAdapter::GetMockDevices() {
  BluetoothAdapter::DeviceList devices;
  for (const auto& it : mock_devices_) {
    devices.push_back(it.get());
  }
  return devices;
}

void MockBluetoothAdapter::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const AdvertisementErrorCallback& error_callback) {
  callback.Run(new MockBluetoothAdvertisement);
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
void MockBluetoothAdapter::SetAdvertisingInterval(
    const base::TimeDelta& min,
    const base::TimeDelta& max,
    const base::Closure& callback,
    const AdvertisementErrorCallback& error_callback) {}
void MockBluetoothAdapter::ResetAdvertising(
    const base::Closure& callback,
    const AdvertisementErrorCallback& error_callback) {}
#endif

}  // namespace device
