// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_remote_gatt_characteristic_bluez.h"

#include <iterator>
#include <limits>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "third_party/cros_system_api/dbus/bluetooth/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

// Stream operator for logging vector<uint8_t>.
std::ostream& operator<<(std::ostream& out, const std::vector<uint8_t> bytes) {
  out << "[";
  for (auto iter = bytes.begin(); iter != bytes.end(); ++iter) {
    out << base::StringPrintf("%02X", *iter);
  }
  return out << "]";
}

}  // namespace

BluetoothRemoteGattCharacteristicBlueZ::BluetoothRemoteGattCharacteristicBlueZ(
    BluetoothRemoteGattServiceBlueZ* service,
    const dbus::ObjectPath& object_path)
    : BluetoothGattCharacteristicBlueZ(object_path),
      has_notify_session_(false),
      service_(service),
      num_of_characteristic_value_read_in_progress_(0) {
  DVLOG(1) << "Creating remote GATT characteristic with identifier: "
           << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattDescriptorClient()
      ->AddObserver(this);

  // Add all known GATT characteristic descriptors.
  const std::vector<dbus::ObjectPath>& gatt_descs =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattDescriptorClient()
          ->GetDescriptors();
  for (auto iter = gatt_descs.begin(); iter != gatt_descs.end(); ++iter)
    GattDescriptorAdded(*iter);
}

BluetoothRemoteGattCharacteristicBlueZ::
    ~BluetoothRemoteGattCharacteristicBlueZ() {
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattDescriptorClient()
      ->RemoveObserver(this);
}

device::BluetoothUUID BluetoothRemoteGattCharacteristicBlueZ::GetUUID() const {
  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path());
  DCHECK(properties);
  return device::BluetoothUUID(properties->uuid.value());
}

device::BluetoothRemoteGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicBlueZ::GetProperties() const {
  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path());
  DCHECK(properties);

  Properties props = PROPERTY_NONE;
  const std::vector<std::string>& flags = properties->flags.value();
  for (auto iter = flags.begin(); iter != flags.end(); ++iter) {
    if (*iter == bluetooth_gatt_characteristic::kFlagBroadcast)
      props |= PROPERTY_BROADCAST;
    if (*iter == bluetooth_gatt_characteristic::kFlagRead)
      props |= PROPERTY_READ;
    if (*iter == bluetooth_gatt_characteristic::kFlagWriteWithoutResponse)
      props |= PROPERTY_WRITE_WITHOUT_RESPONSE;
    if (*iter == bluetooth_gatt_characteristic::kFlagWrite)
      props |= PROPERTY_WRITE;
    if (*iter == bluetooth_gatt_characteristic::kFlagNotify)
      props |= PROPERTY_NOTIFY;
    if (*iter == bluetooth_gatt_characteristic::kFlagIndicate)
      props |= PROPERTY_INDICATE;
    if (*iter == bluetooth_gatt_characteristic::kFlagAuthenticatedSignedWrites)
      props |= PROPERTY_AUTHENTICATED_SIGNED_WRITES;
    if (*iter == bluetooth_gatt_characteristic::kFlagExtendedProperties)
      props |= PROPERTY_EXTENDED_PROPERTIES;
    if (*iter == bluetooth_gatt_characteristic::kFlagReliableWrite)
      props |= PROPERTY_RELIABLE_WRITE;
    if (*iter == bluetooth_gatt_characteristic::kFlagWritableAuxiliaries)
      props |= PROPERTY_WRITABLE_AUXILIARIES;
  }

  return props;
}

device::BluetoothRemoteGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicBlueZ::GetPermissions() const {
  // TODO(armansito): Once BlueZ defines the permissions, return the correct
  // values here.
  return PERMISSION_NONE;
}

const std::vector<uint8_t>& BluetoothRemoteGattCharacteristicBlueZ::GetValue()
    const {
  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path());

  DCHECK(properties);

  return properties->value.value();
}

device::BluetoothRemoteGattService*
BluetoothRemoteGattCharacteristicBlueZ::GetService() const {
  return service_;
}

bool BluetoothRemoteGattCharacteristicBlueZ::IsNotifying() const {
  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path());
  DCHECK(properties);

  // It is not enough to only check notifying.value(). Bluez also
  // needs a notify client/session in order to deliver the
  // notifications.
  return has_notify_session_ && properties->notifying.value();
}

void BluetoothRemoteGattCharacteristicBlueZ::ReadRemoteCharacteristic(
    ValueCallback callback) {
  DVLOG(1) << "Sending GATT characteristic read request to characteristic: "
           << GetIdentifier() << ", UUID: " << GetUUID().canonical_value()
           << ".";

  DCHECK_GE(num_of_characteristic_value_read_in_progress_, 0);
  ++num_of_characteristic_value_read_in_progress_;

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->ReadValue(
          object_path(), std::move(split_callback.first),
          base::BindOnce(&BluetoothRemoteGattCharacteristicBlueZ::OnReadError,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(split_callback.second)));
}

void BluetoothRemoteGattCharacteristicBlueZ::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    WriteType write_type,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(1) << "Sending GATT characteristic write request to characteristic: "
           << GetIdentifier() << ", UUID: " << GetUUID().canonical_value()
           << ", with value: " << value << ", with response: "
           << ((write_type == WriteType::kWithoutResponse) ? "no" : "yes")
           << ".";

  const char* type_option;
  switch (write_type) {
    case WriteType::kWithResponse:
      type_option = bluetooth_gatt_characteristic::kTypeRequest;
      break;
    case WriteType::kWithoutResponse:
      type_option = bluetooth_gatt_characteristic::kTypeCommand;
      break;
  }

  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->WriteValue(
          object_path(), value, type_option, std::move(callback),
          base::BindOnce(&BluetoothRemoteGattCharacteristicBlueZ::OnWriteError,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(error_callback)));
}

void BluetoothRemoteGattCharacteristicBlueZ::
    DeprecatedWriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                        base::OnceClosure callback,
                                        ErrorCallback error_callback) {
  DVLOG(1) << "Sending GATT characteristic write request to characteristic: "
           << GetIdentifier() << ", UUID: " << GetUUID().canonical_value()
           << ", with value: " << value << ".";

  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->WriteValue(
          object_path(), value, "", std::move(callback),
          base::BindOnce(&BluetoothRemoteGattCharacteristicBlueZ::OnWriteError,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(error_callback)));
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothRemoteGattCharacteristicBlueZ::PrepareWriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(1) << "Sending GATT characteristic prepare write request to "
           << "characteristic: " << GetIdentifier()
           << ", UUID: " << GetUUID().canonical_value()
           << ", with value: " << value << ".";

  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->PrepareWriteValue(
          object_path(), value, std::move(callback),
          base::BindOnce(&BluetoothRemoteGattCharacteristicBlueZ::OnWriteError,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(error_callback)));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void BluetoothRemoteGattCharacteristicBlueZ::SubscribeToNotifications(
    device::BluetoothRemoteGattDescriptor* ccc_descriptor,
#if BUILDFLAG(IS_CHROMEOS)
    NotificationType notification_type,
#endif  // BUILDFLAG(IS_CHROMEOS)
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->StartNotify(
          object_path(),
#if BUILDFLAG(IS_CHROMEOS)
          notification_type,
#endif  // BUILDFLAG(IS_CHROMEOS)
          base::BindOnce(
              &BluetoothRemoteGattCharacteristicBlueZ::OnStartNotifySuccess,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          base::BindOnce(
              &BluetoothRemoteGattCharacteristicBlueZ::OnStartNotifyError,
              weak_ptr_factory_.GetWeakPtr(), std::move(error_callback)));
}

void BluetoothRemoteGattCharacteristicBlueZ::UnsubscribeFromNotifications(
    device::BluetoothRemoteGattDescriptor* ccc_descriptor,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->StopNotify(
          object_path(),
          base::BindOnce(
              &BluetoothRemoteGattCharacteristicBlueZ::OnStopNotifySuccess,
              weak_ptr_factory_.GetWeakPtr(), std::move(split_callback.first)),
          base::BindOnce(
              &BluetoothRemoteGattCharacteristicBlueZ::OnStopNotifyError,
              weak_ptr_factory_.GetWeakPtr(),
              std::move(split_callback.second)));
}

void BluetoothRemoteGattCharacteristicBlueZ::GattDescriptorAdded(
    const dbus::ObjectPath& object_path) {
  if (base::Contains(descriptors_, object_path.value())) {
    DVLOG(1) << "Remote GATT characteristic descriptor already exists: "
             << object_path.value();
    return;
  }

  bluez::BluetoothGattDescriptorClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattDescriptorClient()
          ->GetProperties(object_path);
  DCHECK(properties);
  if (properties->characteristic.value() != this->object_path()) {
    DVLOG(3)
        << "Remote GATT descriptor does not belong to this characteristic.";
    return;
  }

  DVLOG(1) << "Adding new remote GATT descriptor for GATT characteristic: "
           << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  // NOTE: Can't use std::make_unique due to private constructor.
  BluetoothRemoteGattDescriptorBlueZ* descriptor =
      new BluetoothRemoteGattDescriptorBlueZ(this, object_path);
  AddDescriptor(base::WrapUnique(descriptor));
  DCHECK(descriptor->GetIdentifier() == object_path.value());
  DCHECK(descriptor->GetUUID().IsValid());
  DCHECK(service_);

  static_cast<BluetoothRemoteGattServiceBlueZ*>(service_)
      ->NotifyDescriptorAddedOrRemoved(this, descriptor, true /* added */);
}

void BluetoothRemoteGattCharacteristicBlueZ::GattDescriptorRemoved(
    const dbus::ObjectPath& object_path) {
  auto iter = descriptors_.find(object_path.value());
  if (iter == descriptors_.end()) {
    DVLOG(2) << "Unknown descriptor removed: " << object_path.value();
    return;
  }

  DVLOG(1) << "Removing remote GATT descriptor from characteristic: "
           << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  auto descriptor = std::move(iter->second);
  auto* descriptor_bluez =
      static_cast<BluetoothRemoteGattDescriptorBlueZ*>(descriptor.get());
  DCHECK(descriptor_bluez->object_path() == object_path);
  descriptors_.erase(iter);

  DCHECK(service_);
  static_cast<BluetoothRemoteGattServiceBlueZ*>(service_)
      ->NotifyDescriptorAddedOrRemoved(this, descriptor_bluez,
                                       false /* added */);
}

void BluetoothRemoteGattCharacteristicBlueZ::GattDescriptorPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  auto iter = descriptors_.find(object_path.value());
  if (iter == descriptors_.end()) {
    DVLOG(2) << "Unknown descriptor removed: " << object_path.value();
    return;
  }

  bluez::BluetoothGattDescriptorClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattDescriptorClient()
          ->GetProperties(object_path);

  DCHECK(properties);

  if (property_name != properties->value.name())
    return;

  DCHECK(service_);
  static_cast<BluetoothRemoteGattServiceBlueZ*>(service_)
      ->NotifyDescriptorValueChanged(
          this,
          static_cast<BluetoothRemoteGattDescriptorBlueZ*>(iter->second.get()),
          properties->value.value());
}

void BluetoothRemoteGattCharacteristicBlueZ::OnStartNotifySuccess(
    base::OnceClosure callback) {
  DVLOG(1) << "Started notifications from characteristic: "
           << object_path().value();
  has_notify_session_ = true;
  std::move(callback).Run();
}

void BluetoothRemoteGattCharacteristicBlueZ::OnStartNotifyError(
    ErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  DVLOG(1) << "Failed to start notifications from characteristic: "
           << object_path().value() << ": " << error_name << ", "
           << error_message;
  std::move(error_callback)
      .Run(
          BluetoothRemoteGattServiceBlueZ::DBusErrorToServiceError(error_name));
}

void BluetoothRemoteGattCharacteristicBlueZ::OnStopNotifySuccess(
    base::OnceClosure callback) {
  has_notify_session_ = false;
  std::move(callback).Run();
}

void BluetoothRemoteGattCharacteristicBlueZ::OnStopNotifyError(
    base::OnceClosure callback,
    const std::string& error_name,
    const std::string& error_message) {
  DVLOG(1) << "Call to stop notifications failed for characteristic: "
           << object_path().value() << ": " << error_name << ", "
           << error_message;

  // Since this is a best effort operation, treat this as success.
  OnStopNotifySuccess(std::move(callback));
}

void BluetoothRemoteGattCharacteristicBlueZ::OnReadError(
    ValueCallback callback,
    const std::string& error_name,
    const std::string& error_message) {
  DVLOG(1) << "Operation failed: " << error_name
           << ", message: " << error_message;
  --num_of_characteristic_value_read_in_progress_;
  DCHECK_GE(num_of_characteristic_value_read_in_progress_, 0);
  std::move(callback).Run(
      std::make_optional(
          BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name)),
      /*value=*/std::vector<uint8_t>());
}

void BluetoothRemoteGattCharacteristicBlueZ::OnWriteError(
    ErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  DVLOG(1) << "Operation failed: " << error_name
           << ", message: " << error_message;
  std::move(error_callback)
      .Run(BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name));
}

}  // namespace bluez
