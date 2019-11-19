// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_adapter_profile_bluez.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/dbus/bluetooth_profile_service_provider.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace bluez {

// static
void BluetoothAdapterProfileBlueZ::Register(
    const device::BluetoothUUID& uuid,
    const bluez::BluetoothProfileManagerClient::Options& options,
    const ProfileRegisteredCallback& success_callback,
    const bluez::BluetoothProfileManagerClient::ErrorCallback& error_callback) {
  std::unique_ptr<BluetoothAdapterProfileBlueZ> profile(
      new BluetoothAdapterProfileBlueZ(uuid));

  VLOG(1) << "Registering profile: " << profile->object_path().value();
  const dbus::ObjectPath& object_path = profile->object_path();
  bluez::BluezDBusManager::Get()
      ->GetBluetoothProfileManagerClient()
      ->RegisterProfile(object_path, uuid.canonical_value(), options,
                        base::Bind(success_callback, base::Passed(&profile)),
                        error_callback);
}

BluetoothAdapterProfileBlueZ::BluetoothAdapterProfileBlueZ(
    const device::BluetoothUUID& uuid)
    : uuid_(uuid) {
  std::string uuid_path;
  base::ReplaceChars(uuid.canonical_value(), ":-", "_", &uuid_path);
  object_path_ =
      dbus::ObjectPath("/org/chromium/bluetooth_profile/" + uuid_path);

  dbus::Bus* system_bus = bluez::BluezDBusManager::Get()->GetSystemBus();
  profile_.reset(bluez::BluetoothProfileServiceProvider::Create(
      system_bus, object_path_, this));
  DCHECK(profile_.get());
}

BluetoothAdapterProfileBlueZ::~BluetoothAdapterProfileBlueZ() = default;

bool BluetoothAdapterProfileBlueZ::SetDelegate(
    const dbus::ObjectPath& device_path,
    bluez::BluetoothProfileServiceProvider::Delegate* delegate) {
  DCHECK(delegate);
  VLOG(1) << "SetDelegate: " << object_path_.value() << " dev "
          << device_path.value();

  if (delegates_.find(device_path.value()) != delegates_.end()) {
    return false;
  }

  delegates_[device_path.value()] = delegate;
  return true;
}

void BluetoothAdapterProfileBlueZ::RemoveDelegate(
    const dbus::ObjectPath& device_path,
    const base::Closure& unregistered_callback) {
  VLOG(1) << object_path_.value() << " dev " << device_path.value()
          << ": RemoveDelegate";

  if (delegates_.find(device_path.value()) == delegates_.end())
    return;

  delegates_.erase(device_path.value());

  if (delegates_.size() != 0)
    return;

  VLOG(1) << device_path.value() << " No delegates left, unregistering.";

  // No users left, release the profile.
  bluez::BluezDBusManager::Get()
      ->GetBluetoothProfileManagerClient()
      ->UnregisterProfile(
          object_path_, unregistered_callback,
          base::Bind(&BluetoothAdapterProfileBlueZ::OnUnregisterProfileError,
                     weak_ptr_factory_.GetWeakPtr(), unregistered_callback));
}

void BluetoothAdapterProfileBlueZ::OnUnregisterProfileError(
    const base::Closure& unregistered_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << this->object_path().value()
               << ": Failed to unregister profile: " << error_name << ": "
               << error_message;

  unregistered_callback.Run();
}

// bluez::BluetoothProfileServiceProvider::Delegate:
void BluetoothAdapterProfileBlueZ::Released() {
  VLOG(1) << object_path_.value() << ": Release";
}

void BluetoothAdapterProfileBlueZ::NewConnection(
    const dbus::ObjectPath& device_path,
    base::ScopedFD fd,
    const bluez::BluetoothProfileServiceProvider::Delegate::Options& options,
    ConfirmationCallback callback) {
  dbus::ObjectPath delegate_path = device_path;

  if (delegates_.find(device_path.value()) == delegates_.end())
    delegate_path = dbus::ObjectPath("");

  if (delegates_.find(delegate_path.value()) == delegates_.end()) {
    VLOG(1) << object_path_.value() << ": New connection for device "
            << device_path.value() << " which has no delegates!";
    std::move(callback).Run(REJECTED);
    return;
  }

  delegates_[delegate_path.value()]->NewConnection(
      device_path, std::move(fd), options, std::move(callback));
}

void BluetoothAdapterProfileBlueZ::RequestDisconnection(
    const dbus::ObjectPath& device_path,
    ConfirmationCallback callback) {
  dbus::ObjectPath delegate_path = device_path;

  if (delegates_.find(device_path.value()) == delegates_.end())
    delegate_path = dbus::ObjectPath("");

  if (delegates_.find(delegate_path.value()) == delegates_.end()) {
    VLOG(1) << object_path_.value() << ": RequestDisconnection for device "
            << device_path.value() << " which has no delegates!";
    return;
  }

  delegates_[delegate_path.value()]->RequestDisconnection(device_path,
                                                          std::move(callback));
}

void BluetoothAdapterProfileBlueZ::Cancel() {
  // Cancel() should only go to a delegate accepting connections.
  if (delegates_.find("") == delegates_.end()) {
    VLOG(1) << object_path_.value() << ": Cancel with no delegate!";
    return;
  }

  delegates_[""]->Cancel();
}

}  // namespace bluez
