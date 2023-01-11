// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_service_provider.h"

#include <algorithm>
#include <iterator>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/dbus/bluetooth_gatt_attribute_value_delegate.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

bool CanWrite(const std::vector<std::string>& flags) {
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_characteristic::kFlagWrite) != flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_characteristic::kFlagWriteWithoutResponse) !=
      flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_characteristic::kFlagReliableWrite) != flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_characteristic::kFlagEncryptWrite) != flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_characteristic::kFlagEncryptAuthenticatedWrite) !=
      flags.end())
    return true;
  return false;
}

bool CanRead(const std::vector<std::string>& flags) {
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_characteristic::kFlagRead) != flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_characteristic::kFlagEncryptRead) != flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_characteristic::kFlagEncryptAuthenticatedRead) !=
      flags.end())
    return true;
  return false;
}

bool CanNotify(const std::vector<std::string>& flags) {
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_characteristic::kFlagNotify) != flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_characteristic::kFlagIndicate) != flags.end())
    return true;
  return false;
}

}  // namespace

FakeBluetoothGattCharacteristicServiceProvider::
    FakeBluetoothGattCharacteristicServiceProvider(
        const dbus::ObjectPath& object_path,
        std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate,
        const std::string& uuid,
        const std::vector<std::string>& flags,
        const dbus::ObjectPath& service_path)
    : object_path_(object_path),
      uuid_(uuid),
      flags_(flags),
      service_path_(service_path),
      delegate_(std::move(delegate)) {
  DVLOG(1) << "Creating Bluetooth GATT characteristic: "
           << object_path_.value();

  DCHECK(object_path_.IsValid());
  DCHECK(service_path_.IsValid());
  DCHECK(!uuid.empty());
  DCHECK(delegate_);
  DCHECK(base::StartsWith(object_path_.value(), service_path_.value() + "/",
                          base::CompareCase::SENSITIVE));

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->RegisterCharacteristicServiceProvider(
      this);
}

FakeBluetoothGattCharacteristicServiceProvider::
    ~FakeBluetoothGattCharacteristicServiceProvider() {
  DVLOG(1) << "Cleaning up Bluetooth GATT characteristic: "
           << object_path_.value();
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->UnregisterCharacteristicServiceProvider(
      this);
}

void FakeBluetoothGattCharacteristicServiceProvider::SendValueChanged(
    const std::vector<uint8_t>& value) {
  DVLOG(1) << "Sent characteristic value changed: " << object_path_.value()
           << " UUID: " << uuid_;
  sent_value_ = value;
}

void FakeBluetoothGattCharacteristicServiceProvider::GetValue(
    const dbus::ObjectPath& device_path,
    device::BluetoothLocalGattService::Delegate::ValueCallback callback) {
  DVLOG(1) << "GATT characteristic value Get request: " << object_path_.value()
           << " UUID: " << uuid_;
  // Check if this characteristic is registered.
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  if (!fake_bluetooth_gatt_manager_client->IsServiceRegistered(service_path_)) {
    DVLOG(1) << "GATT characteristic not registered.";
    std::move(callback).Run(
        device::BluetoothGattService::GattErrorCode::kFailed,
        /*value=*/std::vector<uint8_t>());
    return;
  }

  if (!CanRead(flags_)) {
    DVLOG(1) << "GATT characteristic not readable.";
    std::move(callback).Run(
        device::BluetoothGattService::GattErrorCode::kFailed,
        /*value=*/std::vector<uint8_t>());
    return;
  }

  // Pass on to the delegate.
  DCHECK(delegate_);
  delegate_->GetValue(device_path, std::move(callback));
}

void FakeBluetoothGattCharacteristicServiceProvider::SetValue(
    const dbus::ObjectPath& device_path,
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    device::BluetoothLocalGattService::Delegate::ErrorCallback error_callback) {
  DVLOG(1) << "GATT characteristic value Set request: " << object_path_.value()
           << " UUID: " << uuid_;
  // Check if this characteristic is registered.
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  if (!fake_bluetooth_gatt_manager_client->IsServiceRegistered(service_path_)) {
    DVLOG(1) << "GATT characteristic not registered.";
    std::move(error_callback).Run();
    return;
  }

  if (!CanWrite(flags_)) {
    DVLOG(1) << "GATT characteristic not writeable.";
    std::move(error_callback).Run();
    return;
  }

  // Pass on to the delegate.
  DCHECK(delegate_);
  delegate_->SetValue(device_path, value, std::move(callback),
                      std::move(error_callback));
}

void FakeBluetoothGattCharacteristicServiceProvider::PrepareSetValue(
    const dbus::ObjectPath& device_path,
    const std::vector<uint8_t>& value,
    int offset,
    bool has_subsequent_write,
    base::OnceClosure callback,
    device::BluetoothLocalGattService::Delegate::ErrorCallback error_callback) {
  DVLOG(1) << "GATT characteristic value Prepare Set request: "
           << object_path_.value() << " UUID: " << uuid_;
  // Check if this characteristic is registered.
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  if (!fake_bluetooth_gatt_manager_client->IsServiceRegistered(service_path_)) {
    DVLOG(1) << "GATT characteristic not registered.";
    std::move(error_callback).Run();
    return;
  }

  if (!CanWrite(flags_)) {
    DVLOG(1) << "GATT characteristic not writeable.";
    std::move(error_callback).Run();
    return;
  }

  // Pass on to the delegate.
  DCHECK(delegate_);
  delegate_->PrepareSetValue(device_path, value, offset, has_subsequent_write,
                             std::move(callback), std::move(error_callback));
}

bool FakeBluetoothGattCharacteristicServiceProvider::NotificationsChange(
    const dbus::ObjectPath& device_path,
    bool start) {
  DVLOG(1) << "GATT characteristic value notification request: "
           << object_path_.value() << " UUID: " << uuid_ << " start=" << start;
  // Check if this characteristic is registered.
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  if (!fake_bluetooth_gatt_manager_client->IsServiceRegistered(service_path_)) {
    DVLOG(1) << "GATT characteristic not registered.";
    return false;
  }

  if (!CanNotify(flags_)) {
    DVLOG(1) << "GATT characteristic not notifiable.";
    return false;
  }

  // Pass on to the delegate.
  DCHECK(delegate_);
  start ? delegate_->StartNotifications(device_path,
                                        device::BluetoothGattCharacteristic::
                                            NotificationType::kNotification)
        : delegate_->StopNotifications(device_path);

  return true;
}

const dbus::ObjectPath&
FakeBluetoothGattCharacteristicServiceProvider::object_path() const {
  return object_path_;
}

}  // namespace bluez
