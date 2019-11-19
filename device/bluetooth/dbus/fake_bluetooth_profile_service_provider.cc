// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_profile_service_provider.h"

#include <memory>
#include <utility>

#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_manager_client.h"

namespace bluez {

FakeBluetoothProfileServiceProvider::FakeBluetoothProfileServiceProvider(
    const dbus::ObjectPath& object_path,
    Delegate* delegate)
    : object_path_(object_path), delegate_(delegate) {
  VLOG(1) << "Creating Bluetooth Profile: " << object_path_.value();

  FakeBluetoothProfileManagerClient* fake_bluetooth_profile_manager_client =
      static_cast<FakeBluetoothProfileManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothProfileManagerClient());
  fake_bluetooth_profile_manager_client->RegisterProfileServiceProvider(this);
}

FakeBluetoothProfileServiceProvider::~FakeBluetoothProfileServiceProvider() {
  VLOG(1) << "Cleaning up Bluetooth Profile: " << object_path_.value();

  FakeBluetoothProfileManagerClient* fake_bluetooth_profile_manager_client =
      static_cast<FakeBluetoothProfileManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothProfileManagerClient());
  fake_bluetooth_profile_manager_client->UnregisterProfileServiceProvider(this);
}

void FakeBluetoothProfileServiceProvider::Released() {
  VLOG(1) << object_path_.value() << ": Released";
  delegate_->Released();
}

void FakeBluetoothProfileServiceProvider::NewConnection(
    const dbus::ObjectPath& device_path,
    base::ScopedFD fd,
    const Delegate::Options& options,
    Delegate::ConfirmationCallback callback) {
  VLOG(1) << object_path_.value() << ": NewConnection for "
          << device_path.value();
  delegate_->NewConnection(device_path, std::move(fd), options,
                           std::move(callback));
}

void FakeBluetoothProfileServiceProvider::RequestDisconnection(
    const dbus::ObjectPath& device_path,
    Delegate::ConfirmationCallback callback) {
  VLOG(1) << object_path_.value() << ": RequestDisconnection for "
          << device_path.value();
  delegate_->RequestDisconnection(device_path, std::move(callback));
}

void FakeBluetoothProfileServiceProvider::Cancel() {
  VLOG(1) << object_path_.value() << ": Cancel";
  delegate_->Cancel();
}

}  // namespace bluez
