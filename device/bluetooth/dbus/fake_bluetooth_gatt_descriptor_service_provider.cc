// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_service_provider.h"

#include <algorithm>
#include <iterator>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/dbus/bluetooth_gatt_attribute_value_delegate.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

bool CanWrite(const std::vector<std::string>& flags) {
  if (find(flags.begin(), flags.end(), bluetooth_gatt_descriptor::kFlagWrite) !=
      flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_descriptor::kFlagEncryptWrite) != flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_descriptor::kFlagEncryptAuthenticatedWrite) !=
      flags.end())
    return true;
  return false;
}

bool CanRead(const std::vector<std::string>& flags) {
  if (find(flags.begin(), flags.end(), bluetooth_gatt_descriptor::kFlagRead) !=
      flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_descriptor::kFlagEncryptRead) != flags.end())
    return true;
  if (find(flags.begin(), flags.end(),
           bluetooth_gatt_descriptor::kFlagEncryptAuthenticatedRead) !=
      flags.end())
    return true;
  return false;
}

}  // namespace

FakeBluetoothGattDescriptorServiceProvider::
    FakeBluetoothGattDescriptorServiceProvider(
        const dbus::ObjectPath& object_path,
        std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate,
        const std::string& uuid,
        const std::vector<std::string>& flags,
        const dbus::ObjectPath& characteristic_path)
    : object_path_(object_path),
      uuid_(uuid),
      flags_(flags),
      characteristic_path_(characteristic_path),
      delegate_(std::move(delegate)) {
  DVLOG(1) << "Creating Bluetooth GATT descriptor: " << object_path_.value();

  DCHECK(object_path_.IsValid());
  DCHECK(characteristic_path_.IsValid());
  DCHECK(!uuid.empty());
  DCHECK(delegate_);
  DCHECK(base::StartsWith(object_path_.value(),
                          characteristic_path_.value() + "/",
                          base::CompareCase::SENSITIVE));
  // TODO(rkc): Do something with |flags|.

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->RegisterDescriptorServiceProvider(this);
}

FakeBluetoothGattDescriptorServiceProvider::
    ~FakeBluetoothGattDescriptorServiceProvider() {
  DVLOG(1) << "Cleaning up Bluetooth GATT descriptor: " << object_path_.value();

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->UnregisterDescriptorServiceProvider(this);
}

void FakeBluetoothGattDescriptorServiceProvider::SendValueChanged(
    const std::vector<uint8_t>& value) {
  DVLOG(1) << "Sent descriptor value changed: " << object_path_.value()
           << " UUID: " << uuid_;
}

void FakeBluetoothGattDescriptorServiceProvider::GetValue(
    const dbus::ObjectPath& device_path,
    device::BluetoothLocalGattService::Delegate::ValueCallback callback) {
  DVLOG(1) << "GATT descriptor value Get request: " << object_path_.value()
           << " UUID: " << uuid_;
  // Check if this descriptor is registered.
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  FakeBluetoothGattCharacteristicServiceProvider* characteristic =
      fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
          characteristic_path_);
  if (!characteristic) {
    DVLOG(1) << "GATT characteristic for descriptor does not exist: "
             << characteristic_path_.value();
    return;
  }
  if (!fake_bluetooth_gatt_manager_client->IsServiceRegistered(
          characteristic->service_path())) {
    DVLOG(1) << "GATT descriptor not registered.";
    std::move(callback).Run(
        device::BluetoothGattService::GattErrorCode::kFailed,
        /*value=*/std::vector<uint8_t>());
    return;
  }

  if (!CanRead(flags_)) {
    std::move(callback).Run(
        device::BluetoothGattService::GattErrorCode::kFailed,
        /*value=*/std::vector<uint8_t>());
    return;
  }

  // Pass on to the delegate.
  DCHECK(delegate_);
  delegate_->GetValue(device_path, std::move(callback));
}

void FakeBluetoothGattDescriptorServiceProvider::SetValue(
    const dbus::ObjectPath& device_path,
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    device::BluetoothLocalGattService::Delegate::ErrorCallback error_callback) {
  DVLOG(1) << "GATT descriptor value Set request: " << object_path_.value()
           << " UUID: " << uuid_;

  // Check if this descriptor is registered.
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  FakeBluetoothGattCharacteristicServiceProvider* characteristic =
      fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
          characteristic_path_);
  if (!characteristic) {
    DVLOG(1) << "GATT characteristic for descriptor does not exist: "
             << characteristic_path_.value();
    return;
  }
  if (!fake_bluetooth_gatt_manager_client->IsServiceRegistered(
          characteristic->service_path())) {
    DVLOG(1) << "GATT descriptor not registered.";
    std::move(error_callback).Run();
    return;
  }

  if (!CanWrite(flags_)) {
    DVLOG(1) << "GATT descriptor not writeable.";
    std::move(error_callback).Run();
    return;
  }

  // Pass on to the delegate.
  DCHECK(delegate_);
  delegate_->SetValue(device_path, value, std::move(callback),
                      std::move(error_callback));
}

const dbus::ObjectPath&
FakeBluetoothGattDescriptorServiceProvider::object_path() const {
  return object_path_;
}

}  // namespace bluez
