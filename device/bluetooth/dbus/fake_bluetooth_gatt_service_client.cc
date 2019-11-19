// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

// static
const char FakeBluetoothGattServiceClient::kHeartRateServicePathComponent[] =
    "service0000";
const char FakeBluetoothGattServiceClient::kBatteryServicePathComponent[] =
    "service0001";
const char FakeBluetoothGattServiceClient::kGenericAccessServiceUUID[] =
    "00001800-0000-1000-8000-00805f9b34fb";
const char FakeBluetoothGattServiceClient::kHeartRateServiceUUID[] =
    "0000180d-0000-1000-8000-00805f9b34fb";
const char FakeBluetoothGattServiceClient::kBatteryServiceUUID[] =
    "0000180f-0000-1000-8000-00805f9b34fb";

FakeBluetoothGattServiceClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothGattServiceClient::Properties(
          NULL,
          bluetooth_gatt_service::kBluetoothGattServiceInterface,
          callback) {}

FakeBluetoothGattServiceClient::Properties::~Properties() = default;

void FakeBluetoothGattServiceClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeBluetoothGattServiceClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeBluetoothGattServiceClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  std::move(callback).Run(false);
}

FakeBluetoothGattServiceClient::FakeBluetoothGattServiceClient() {}

FakeBluetoothGattServiceClient::~FakeBluetoothGattServiceClient() = default;

void FakeBluetoothGattServiceClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothGattServiceClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothGattServiceClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath> FakeBluetoothGattServiceClient::GetServices() {
  std::vector<dbus::ObjectPath> paths;
  if (heart_rate_service_properties_.get()) {
    DCHECK(!heart_rate_service_path_.empty());
    paths.push_back(dbus::ObjectPath(heart_rate_service_path_));
  }
  if (battery_service_properties_.get()) {
    DCHECK(!battery_service_path_.empty());
    paths.push_back(dbus::ObjectPath(battery_service_path_));
  }
  return paths;
}

FakeBluetoothGattServiceClient::Properties*
FakeBluetoothGattServiceClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  if (object_path.value() == heart_rate_service_path_)
    return heart_rate_service_properties_.get();
  if (object_path.value() == battery_service_path_)
    return battery_service_properties_.get();
  return NULL;
}

void FakeBluetoothGattServiceClient::ExposeHeartRateService(
    const dbus::ObjectPath& device_path) {
  if (IsHeartRateVisible()) {
    DCHECK(!heart_rate_service_path_.empty());
    VLOG(1) << "Fake Heart Rate Service already exposed.";
    return;
  }
  VLOG(2) << "Exposing fake Heart Rate Service.";
  heart_rate_service_path_ =
      device_path.value() + "/" + kHeartRateServicePathComponent;
  heart_rate_service_properties_.reset(new Properties(base::Bind(
      &FakeBluetoothGattServiceClient::OnPropertyChanged,
      base::Unretained(this), dbus::ObjectPath(heart_rate_service_path_))));
  heart_rate_service_properties_->uuid.ReplaceValue(kHeartRateServiceUUID);
  heart_rate_service_properties_->device.ReplaceValue(device_path);
  heart_rate_service_properties_->primary.ReplaceValue(true);

  NotifyServiceAdded(GetHeartRateServicePath());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeBluetoothGattServiceClient::ExposeHeartRateCharacteristics,
          weak_ptr_factory_.GetWeakPtr()));
}

void FakeBluetoothGattServiceClient::HideHeartRateService() {
  if (!IsHeartRateVisible()) {
    DCHECK(heart_rate_service_path_.empty());
    VLOG(1) << "Fake Heart Rate Service already hidden.";
    return;
  }
  VLOG(2) << "Hiding fake Heart Rate Service.";
  FakeBluetoothGattCharacteristicClient* char_client =
      static_cast<FakeBluetoothGattCharacteristicClient*>(
          bluez::BluezDBusManager::Get()
              ->GetBluetoothGattCharacteristicClient());
  char_client->HideHeartRateCharacteristics();

  // Notify observers before deleting the properties structure so that it
  // can be accessed from the observer method.
  NotifyServiceRemoved(dbus::ObjectPath(heart_rate_service_path_));

  heart_rate_service_properties_.reset();
  heart_rate_service_path_.clear();
}

void FakeBluetoothGattServiceClient::ExposeBatteryService(
    const dbus::ObjectPath& device_path) {
  if (IsBatteryServiceVisible()) {
    DCHECK(!battery_service_path_.empty());
    VLOG(1) << "Fake Battery Service already exposed.";
    return;
  }

  VLOG(2) << "Exposing fake Battery Service.";
  battery_service_path_ =
      device_path.value() + "/" + kBatteryServicePathComponent;
  battery_service_properties_.reset(new Properties(base::Bind(
      &FakeBluetoothGattServiceClient::OnPropertyChanged,
      base::Unretained(this), dbus::ObjectPath(battery_service_path_))));
  battery_service_properties_->uuid.ReplaceValue(kBatteryServiceUUID);
  battery_service_properties_->device.ReplaceValue(device_path);
  battery_service_properties_->primary.ReplaceValue(true);

  NotifyServiceAdded(GetBatteryServicePath());
}

bool FakeBluetoothGattServiceClient::IsHeartRateVisible() const {
  return !!heart_rate_service_properties_.get();
}

bool FakeBluetoothGattServiceClient::IsBatteryServiceVisible() const {
  return !!battery_service_properties_.get();
}

dbus::ObjectPath FakeBluetoothGattServiceClient::GetHeartRateServicePath()
    const {
  return dbus::ObjectPath(heart_rate_service_path_);
}

dbus::ObjectPath FakeBluetoothGattServiceClient::GetBatteryServicePath() const {
  return dbus::ObjectPath(battery_service_path_);
}

void FakeBluetoothGattServiceClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  VLOG(2) << "Fake GATT Service property changed: " << object_path.value()
          << ": " << property_name;
  for (auto& observer : observers_)
    observer.GattServicePropertyChanged(object_path, property_name);
}

void FakeBluetoothGattServiceClient::NotifyServiceAdded(
    const dbus::ObjectPath& object_path) {
  VLOG(2) << "GATT service added: " << object_path.value();
  for (auto& observer : observers_)
    observer.GattServiceAdded(object_path);
}

void FakeBluetoothGattServiceClient::NotifyServiceRemoved(
    const dbus::ObjectPath& object_path) {
  VLOG(2) << "GATT service removed: " << object_path.value();
  for (auto& observer : observers_)
    observer.GattServiceRemoved(object_path);
}

void FakeBluetoothGattServiceClient::ExposeHeartRateCharacteristics() {
  if (!IsHeartRateVisible()) {
    VLOG(2) << "Heart Rate service not visible. Not exposing characteristics.";
    return;
  }
  FakeBluetoothGattCharacteristicClient* char_client =
      static_cast<FakeBluetoothGattCharacteristicClient*>(
          bluez::BluezDBusManager::Get()
              ->GetBluetoothGattCharacteristicClient());
  char_client->ExposeHeartRateCharacteristics(
      dbus::ObjectPath(heart_rate_service_path_));
}

}  // namespace bluez
