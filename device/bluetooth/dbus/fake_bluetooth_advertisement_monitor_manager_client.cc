// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_manager_client.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

FakeBluetoothAdvertisementMonitorManagerClient::
    FakeBluetoothAdvertisementMonitorManagerClient() = default;

FakeBluetoothAdvertisementMonitorManagerClient::
    ~FakeBluetoothAdvertisementMonitorManagerClient() = default;

void FakeBluetoothAdvertisementMonitorManagerClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothAdvertisementMonitorManagerClient::RegisterMonitor(
    const dbus::ObjectPath& application,
    const dbus::ObjectPath& adapter,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  std::move(callback).Run();
}

void FakeBluetoothAdvertisementMonitorManagerClient::UnregisterMonitor(
    const dbus::ObjectPath& application,
    const dbus::ObjectPath& adapter,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

BluetoothAdvertisementMonitorManagerClient::Properties*
FakeBluetoothAdvertisementMonitorManagerClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  return properties_.get();
}

void FakeBluetoothAdvertisementMonitorManagerClient::AddObserver(
    Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothAdvertisementMonitorManagerClient::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeBluetoothAdvertisementMonitorManagerClient::
    RegisterApplicationServiceProvider(
        FakeBluetoothAdvertisementMonitorApplicationServiceProvider* provider) {
  DCHECK(provider);
  application_provider_ = provider;
  InitializeProperties();
}

void FakeBluetoothAdvertisementMonitorManagerClient::InitializeProperties() {
  properties_ = std::make_unique<Properties>(
      nullptr,
      bluetooth_advertisement_monitor_manager::
          kBluetoothAdvertisementMonitorManagerInterface,
      base::BindRepeating(
          &FakeBluetoothAdvertisementMonitorManagerClient::OnPropertyChanged,
          weak_ptr_factory_.GetWeakPtr(),
          /*object_path=*/dbus::ObjectPath("")));
}

void FakeBluetoothAdvertisementMonitorManagerClient::RemoveProperties() {
  properties_.reset();
}

void FakeBluetoothAdvertisementMonitorManagerClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DVLOG(2) << "Bluetooth Advertisement Monitor Client property changed: "
           << object_path.value() << ": " << property_name;

  if (property_name ==
      bluetooth_advertisement_monitor_manager::kSupportedFeatures) {
    for (auto& observer : observers_)
      observer.SupportedAdvertisementMonitorFeaturesChanged();
  }
}

}  // namespace bluez
