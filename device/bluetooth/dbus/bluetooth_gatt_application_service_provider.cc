// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_application_service_provider.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_application_service_provider_impl.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_delegate_wrapper.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_delegate_wrapper.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_application_service_provider.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

const std::vector<std::string> FlagsFromPropertiesAndPermissions(
    device::BluetoothGattCharacteristic::Properties properties,
    device::BluetoothGattCharacteristic::Permissions permissions) {
  static_assert(
      device::BluetoothGattCharacteristic::NUM_PROPERTY == 1 << 14,
      "Update required if the number of characteristic properties changes.");
  std::vector<std::string> flags;
  if (properties & device::BluetoothGattCharacteristic::PROPERTY_BROADCAST)
    flags.push_back(bluetooth_gatt_characteristic::kFlagBroadcast);
  if (properties & device::BluetoothGattCharacteristic::PROPERTY_READ)
    flags.push_back(bluetooth_gatt_characteristic::kFlagRead);
  if (properties &
      device::BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE)
    flags.push_back(bluetooth_gatt_characteristic::kFlagWriteWithoutResponse);
  if (properties & device::BluetoothGattCharacteristic::PROPERTY_WRITE)
    flags.push_back(bluetooth_gatt_characteristic::kFlagWrite);
  if (properties & device::BluetoothGattCharacteristic::PROPERTY_NOTIFY)
    flags.push_back(bluetooth_gatt_characteristic::kFlagNotify);
  if (properties & device::BluetoothGattCharacteristic::PROPERTY_INDICATE)
    flags.push_back(bluetooth_gatt_characteristic::kFlagIndicate);
  if (properties &
      device::BluetoothGattCharacteristic::PROPERTY_AUTHENTICATED_SIGNED_WRITES)
    flags.push_back(
        bluetooth_gatt_characteristic::kFlagAuthenticatedSignedWrites);
  if (properties &
      device::BluetoothGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES)
    flags.push_back(bluetooth_gatt_characteristic::kFlagExtendedProperties);
  if (properties & device::BluetoothGattCharacteristic::PROPERTY_RELIABLE_WRITE)
    flags.push_back(bluetooth_gatt_characteristic::kFlagReliableWrite);
  if (properties &
      device::BluetoothGattCharacteristic::PROPERTY_WRITABLE_AUXILIARIES)
    flags.push_back(bluetooth_gatt_characteristic::kFlagWritableAuxiliaries);
  if (properties & device::BluetoothGattCharacteristic::PROPERTY_READ_ENCRYPTED)
    flags.push_back(bluetooth_gatt_characteristic::kFlagEncryptRead);
  if (properties &
      device::BluetoothGattCharacteristic::PROPERTY_WRITE_ENCRYPTED)
    flags.push_back(bluetooth_gatt_characteristic::kFlagEncryptWrite);
  if (properties & device::BluetoothGattCharacteristic::
                       PROPERTY_READ_ENCRYPTED_AUTHENTICATED)
    flags.push_back(
        bluetooth_gatt_characteristic::kFlagEncryptAuthenticatedRead);
  if (properties & device::BluetoothGattCharacteristic::
                       PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED)
    flags.push_back(
        bluetooth_gatt_characteristic::kFlagEncryptAuthenticatedWrite);
  if (permissions & device::BluetoothGattCharacteristic::PERMISSION_READ)
    flags.push_back(bluetooth_gatt_characteristic::kFlagPermissionRead);
  if (permissions & device::BluetoothGattCharacteristic::PERMISSION_WRITE)
    flags.push_back(bluetooth_gatt_characteristic::kFlagPermissionWrite);
  if (permissions &
      device::BluetoothGattCharacteristic::PERMISSION_READ_ENCRYPTED)
    flags.push_back(bluetooth_gatt_characteristic::kFlagPermissionEncryptRead);
  if (permissions &
      device::BluetoothGattCharacteristic::PERMISSION_WRITE_ENCRYPTED)
    flags.push_back(bluetooth_gatt_characteristic::kFlagPermissionEncryptWrite);
  if (permissions & device::BluetoothGattCharacteristic::
                        PERMISSION_READ_ENCRYPTED_AUTHENTICATED)
    flags.push_back(
        bluetooth_gatt_characteristic::kFlagPermissionAuthenticatedRead);
  if (permissions & device::BluetoothGattCharacteristic::
                        PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED)
    flags.push_back(
        bluetooth_gatt_characteristic::kFlagPermissionAuthenticatedWrite);
  return flags;
}

const std::vector<std::string> FlagsFromPermissions(
    device::BluetoothGattCharacteristic::Permissions permissions) {
  static_assert(
      device::BluetoothGattCharacteristic::NUM_PERMISSION == 1 << 6,
      "Update required if the number of attribute permissions changes.");
  std::vector<std::string> flags;
  if (permissions & device::BluetoothGattCharacteristic::PERMISSION_READ)
    flags.push_back(bluetooth_gatt_descriptor::kFlagRead);
  if (permissions & device::BluetoothGattCharacteristic::PERMISSION_WRITE)
    flags.push_back(bluetooth_gatt_descriptor::kFlagWrite);
  if (permissions &
      device::BluetoothGattCharacteristic::PERMISSION_READ_ENCRYPTED)
    flags.push_back(bluetooth_gatt_descriptor::kFlagEncryptRead);
  if (permissions &
      device::BluetoothGattCharacteristic::PERMISSION_WRITE_ENCRYPTED)
    flags.push_back(bluetooth_gatt_descriptor::kFlagEncryptWrite);
  if (permissions & device::BluetoothGattCharacteristic::
                        PERMISSION_READ_ENCRYPTED_AUTHENTICATED)
    flags.push_back(bluetooth_gatt_descriptor::kFlagEncryptAuthenticatedRead);
  if (permissions & device::BluetoothGattCharacteristic::
                        PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED)
    flags.push_back(bluetooth_gatt_descriptor::kFlagEncryptAuthenticatedWrite);
  return flags;
}

}  // namespace

BluetoothGattApplicationServiceProvider::
    BluetoothGattApplicationServiceProvider() = default;

BluetoothGattApplicationServiceProvider::
    ~BluetoothGattApplicationServiceProvider() = default;

void BluetoothGattApplicationServiceProvider::CreateAttributeServiceProviders(
    dbus::Bus* bus,
    const std::map<dbus::ObjectPath, BluetoothLocalGattServiceBlueZ*>&
        services) {
  for (const auto& service : services) {
    service_providers_.push_back(
        base::WrapUnique(BluetoothGattServiceServiceProvider::Create(
            bus, service.second->object_path(),
            service.second->GetUUID().value(), service.second->IsPrimary(),
            std::vector<dbus::ObjectPath>())));
    for (const auto& characteristic : service.second->GetCharacteristics()) {
      characteristic_providers_.push_back(
          base::WrapUnique(BluetoothGattCharacteristicServiceProvider::Create(
              bus, characteristic.second->object_path(),
              std::make_unique<BluetoothGattCharacteristicDelegateWrapper>(
                  service.second, characteristic.second.get()),
              characteristic.second->GetUUID().value(),
              FlagsFromPropertiesAndPermissions(
                  characteristic.second->GetProperties(),
                  characteristic.second->GetPermissions()),
              service.second->object_path())));
      for (const auto& descriptor : characteristic.second->GetDescriptors()) {
        descriptor_providers_.push_back(
            base::WrapUnique(BluetoothGattDescriptorServiceProvider::Create(
                bus, descriptor->object_path(),
                std::make_unique<BluetoothGattDescriptorDelegateWrapper>(
                    service.second, descriptor.get()),
                descriptor->GetUUID().value(),
                FlagsFromPermissions(descriptor->GetPermissions()),
                characteristic.second->object_path())));
      }
    }
  }
}

void BluetoothGattApplicationServiceProvider::SendValueChanged(
    const dbus::ObjectPath& characteristic_path,
    const std::vector<uint8_t>& value) {
  auto characteristic = std::find_if(
      characteristic_providers_.begin(), characteristic_providers_.end(),
      [&](const std::unique_ptr<BluetoothGattCharacteristicServiceProvider>&
              p) { return p->object_path() == characteristic_path; });
  if (characteristic == characteristic_providers_.end()) {
    LOG(ERROR) << "Couldn't find characteristic provider for: "
               << characteristic_path.value();
    return;
  }
  characteristic->get()->SendValueChanged(value);
}

// static
std::unique_ptr<BluetoothGattApplicationServiceProvider>
BluetoothGattApplicationServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path,
    const std::map<dbus::ObjectPath, BluetoothLocalGattServiceBlueZ*>&
        services) {
  if (!bluez::BluezDBusManager::Get()->IsUsingFakes()) {
    return base::WrapUnique(new BluetoothGattApplicationServiceProviderImpl(
        bus, object_path, services));
  }
  return std::make_unique<FakeBluetoothGattApplicationServiceProvider>(
      object_path, services);
}

}  // namespace bluez
