// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_advertisement_bluez.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "dbus/bus.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

void UnregisterFailure(device::BluetoothAdvertisement::ErrorCode error) {
  LOG(ERROR)
      << "BluetoothAdvertisementBlueZ::Unregister failed with error code = "
      << error;
}

device::BluetoothAdvertisement::ErrorCode GetErrorCodeFromErrorStrings(
    const std::string& error_name,
    const std::string& error_message) {
  if (error_name == bluetooth_advertising_manager::kErrorFailed ||
      error_name == bluetooth_advertising_manager::kErrorAlreadyExists) {
    return device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_ALREADY_EXISTS;
  } else if (error_name ==
             bluetooth_advertising_manager::kErrorInvalidArguments) {
    return device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_INVALID_LENGTH;
  } else if (error_name == bluetooth_advertising_manager::kErrorDoesNotExist) {
    return device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_DOES_NOT_EXIST;
  }
  return device::BluetoothAdvertisement::ErrorCode::
      INVALID_ADVERTISEMENT_ERROR_CODE;
}

void RegisterErrorCallbackConnector(
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(ERROR) << "Error while registering advertisement. error_name = "
             << error_name << ", error_message = " << error_message;
  std::move(error_callback)
      .Run(GetErrorCodeFromErrorStrings(error_name, error_message));
}

void UnregisterErrorCallbackConnector(
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << "Error while unregistering advertisement. error_name = "
               << error_name << ", error_message = " << error_message;
  std::move(error_callback)
      .Run(GetErrorCodeFromErrorStrings(error_name, error_message));
}

}  // namespace

namespace bluez {

BluetoothAdvertisementBlueZ::BluetoothAdvertisementBlueZ(
    std::unique_ptr<device::BluetoothAdvertisement::Data> data,
    scoped_refptr<BluetoothAdapterBlueZ> adapter)
    : adapter_path_(adapter->object_path()) {
  // Generate a new object path - make sure that we strip any -'s from the
  // generated GUID string since object paths can only contain alphanumeric
  // characters and _ characters.
  std::string GuidString = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::RemoveChars(GuidString, "-", &GuidString);
  dbus::ObjectPath advertisement_object_path =
      dbus::ObjectPath("/org/chromium/bluetooth_advertisement/" + GuidString);

  DCHECK(bluez::BluezDBusManager::Get());
  provider_ = bluez::BluetoothLEAdvertisementServiceProvider::Create(
      bluez::BluezDBusManager::Get()->GetSystemBus(), advertisement_object_path,
      this, adapter->IsExtendedAdvertisementsAvailable(),
      static_cast<
          bluez::BluetoothLEAdvertisementServiceProvider::AdvertisementType>(
          data->type()),
      data->service_uuids(), data->manufacturer_data(), data->solicit_uuids(),
      data->service_data(), data->scan_response_data());
}

void BluetoothAdvertisementBlueZ::Register(
    SuccessCallback success_callback,
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback) {
  DCHECK(bluez::BluezDBusManager::Get());
  bluez::BluezDBusManager::Get()
      ->GetBluetoothLEAdvertisingManagerClient()
      ->RegisterAdvertisement(adapter_path_, provider_->object_path(),
                              std::move(success_callback),
                              base::BindOnce(&RegisterErrorCallbackConnector,
                                             std::move(error_callback)));
}

BluetoothAdvertisementBlueZ::~BluetoothAdvertisementBlueZ() {
  Unregister(base::DoNothing(), base::BindOnce(&UnregisterFailure));
}

void BluetoothAdvertisementBlueZ::Unregister(SuccessCallback success_callback,
                                             ErrorCallback error_callback) {
  // If we don't have a provider, that means we have already been unregistered,
  // return an error.
  if (!provider_) {
    std::move(error_callback)
        .Run(device::BluetoothAdvertisement::ErrorCode::
                 ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
    return;
  }

  DCHECK(bluez::BluezDBusManager::Get());
  bluez::BluezDBusManager::Get()
      ->GetBluetoothLEAdvertisingManagerClient()
      ->UnregisterAdvertisement(
          adapter_path_, provider_->object_path(), std::move(success_callback),
          base::BindOnce(&UnregisterErrorCallbackConnector,
                         std::move(error_callback)));
  provider_.reset();
}

void BluetoothAdvertisementBlueZ::Released() {
  LOG(WARNING) << "Advertisement released.";
  provider_.reset();
  for (auto& observer : observers_)
    observer.AdvertisementReleased(this);
}

}  // namespace bluez
