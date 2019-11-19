// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertisement_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertising_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

constexpr char kAdvertisingManagerPath[] = "/fake/hci0";
// According to the Bluetooth spec, these are the min and max values possible
// for advertising interval. Core 4.2 Spec, Vol 2, Part E, Section 7.8.5.
constexpr uint16_t kMinIntervalMs = 20;
constexpr uint16_t kMaxIntervalMs = 10240;

}  // namespace

FakeBluetoothLEAdvertisingManagerClient::
    FakeBluetoothLEAdvertisingManagerClient() = default;

FakeBluetoothLEAdvertisingManagerClient::
    ~FakeBluetoothLEAdvertisingManagerClient() = default;

void FakeBluetoothLEAdvertisingManagerClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothLEAdvertisingManagerClient::AddObserver(Observer* observer) {}

void FakeBluetoothLEAdvertisingManagerClient::RemoveObserver(
    Observer* observer) {}

void FakeBluetoothLEAdvertisingManagerClient::RegisterAdvertisement(
    const dbus::ObjectPath& manager_object_path,
    const dbus::ObjectPath& advertisement_object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  VLOG(1) << "RegisterAdvertisment: " << advertisement_object_path.value();

  if (manager_object_path != dbus::ObjectPath(kAdvertisingManagerPath)) {
    std::move(error_callback)
        .Run(kNoResponseError, "Invalid Advertising Manager path.");
    return;
  }

  auto iter = service_provider_map_.find(advertisement_object_path);
  if (iter == service_provider_map_.end()) {
    std::move(error_callback)
        .Run(bluetooth_advertising_manager::kErrorInvalidArguments,
             "Advertisement object not registered");
  } else if (currently_registered_.size() >= kMaxBluezAdvertisements) {
    std::move(error_callback)
        .Run(bluetooth_advertising_manager::kErrorFailed,
             "Maximum advertisements reached");
  } else {
    currently_registered_.push_back(advertisement_object_path);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  }
}

void FakeBluetoothLEAdvertisingManagerClient::UnregisterAdvertisement(
    const dbus::ObjectPath& manager_object_path,
    const dbus::ObjectPath& advertisement_object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  VLOG(1) << "UnregisterAdvertisment: " << advertisement_object_path.value();

  if (manager_object_path != dbus::ObjectPath(kAdvertisingManagerPath)) {
    std::move(error_callback)
        .Run(kNoResponseError, "Invalid Advertising Manager path.");
    return;
  }

  auto service_iter = service_provider_map_.find(advertisement_object_path);
  auto reg_iter =
      std::find(currently_registered_.begin(), currently_registered_.end(),
                advertisement_object_path);

  if (service_iter == service_provider_map_.end()) {
    std::move(error_callback)
        .Run(bluetooth_advertising_manager::kErrorDoesNotExist,
             "Advertisement not registered");
  } else if (reg_iter == currently_registered_.end()) {
    std::move(error_callback)
        .Run(bluetooth_advertising_manager::kErrorDoesNotExist,
             "Does not exist");
  } else {
    currently_registered_.erase(reg_iter);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  }
}

void FakeBluetoothLEAdvertisingManagerClient::SetAdvertisingInterval(
    const dbus::ObjectPath& object_path,
    uint16_t min_interval_ms,
    uint16_t max_interval_ms,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (min_interval_ms < kMinIntervalMs || max_interval_ms > kMaxIntervalMs ||
      min_interval_ms > max_interval_ms) {
    std::move(error_callback)
        .Run(bluetooth_advertising_manager::kErrorInvalidArguments,
             "Invalid interval.");
    return;
  }
  std::move(callback).Run();
}

void FakeBluetoothLEAdvertisingManagerClient::ResetAdvertising(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  currently_registered_.clear();
  service_provider_map_.clear();
  std::move(callback).Run();
}

void FakeBluetoothLEAdvertisingManagerClient::
    RegisterAdvertisementServiceProvider(
        FakeBluetoothLEAdvertisementServiceProvider* service_provider) {
  service_provider_map_[service_provider->object_path_] = service_provider;
}

void FakeBluetoothLEAdvertisingManagerClient::
    UnregisterAdvertisementServiceProvider(
        FakeBluetoothLEAdvertisementServiceProvider* service_provider) {
  auto iter = service_provider_map_.find(service_provider->object_path_);
  if (iter != service_provider_map_.end() && iter->second == service_provider)
    service_provider_map_.erase(iter);
}

}  // namespace bluez
