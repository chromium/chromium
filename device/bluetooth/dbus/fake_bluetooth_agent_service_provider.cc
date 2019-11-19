// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_agent_service_provider.h"

#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"

namespace bluez {

FakeBluetoothAgentServiceProvider::FakeBluetoothAgentServiceProvider(
    const dbus::ObjectPath& object_path,
    Delegate* delegate)
    : object_path_(object_path), delegate_(delegate) {
  VLOG(1) << "Creating Bluetooth Agent: " << object_path_.value();

  FakeBluetoothAgentManagerClient* fake_bluetooth_agent_manager_client =
      static_cast<FakeBluetoothAgentManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothAgentManagerClient());
  fake_bluetooth_agent_manager_client->RegisterAgentServiceProvider(this);
}

FakeBluetoothAgentServiceProvider::~FakeBluetoothAgentServiceProvider() {
  VLOG(1) << "Cleaning up Bluetooth Agent: " << object_path_.value();

  FakeBluetoothAgentManagerClient* fake_bluetooth_agent_manager_client =
      static_cast<FakeBluetoothAgentManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothAgentManagerClient());
  fake_bluetooth_agent_manager_client->UnregisterAgentServiceProvider(this);
}

void FakeBluetoothAgentServiceProvider::Release() {
  VLOG(1) << object_path_.value() << ": Release";
  delegate_->Released();
}

void FakeBluetoothAgentServiceProvider::RequestPinCode(
    const dbus::ObjectPath& device_path,
    Delegate::PinCodeCallback callback) {
  VLOG(1) << object_path_.value() << ": RequestPinCode for "
          << device_path.value();
  delegate_->RequestPinCode(device_path, std::move(callback));
}

void FakeBluetoothAgentServiceProvider::DisplayPinCode(
    const dbus::ObjectPath& device_path,
    const std::string& pincode) {
  VLOG(1) << object_path_.value() << ": DisplayPincode " << pincode << " for "
          << device_path.value();
  delegate_->DisplayPinCode(device_path, pincode);
}

void FakeBluetoothAgentServiceProvider::RequestPasskey(
    const dbus::ObjectPath& device_path,
    Delegate::PasskeyCallback callback) {
  VLOG(1) << object_path_.value() << ": RequestPasskey for "
          << device_path.value();
  delegate_->RequestPasskey(device_path, std::move(callback));
}

void FakeBluetoothAgentServiceProvider::DisplayPasskey(
    const dbus::ObjectPath& device_path,
    uint32_t passkey,
    int16_t entered) {
  VLOG(1) << object_path_.value() << ": DisplayPasskey " << passkey << " ("
          << entered << " entered) for " << device_path.value();
  delegate_->DisplayPasskey(device_path, passkey, entered);
}

void FakeBluetoothAgentServiceProvider::RequestConfirmation(
    const dbus::ObjectPath& device_path,
    uint32_t passkey,
    Delegate::ConfirmationCallback callback) {
  VLOG(1) << object_path_.value() << ": RequestConfirmation " << passkey
          << " for " << device_path.value();
  delegate_->RequestConfirmation(device_path, passkey, std::move(callback));
}

void FakeBluetoothAgentServiceProvider::RequestAuthorization(
    const dbus::ObjectPath& device_path,
    Delegate::ConfirmationCallback callback) {
  VLOG(1) << object_path_.value() << ": RequestAuthorization for "
          << device_path.value();
  delegate_->RequestAuthorization(device_path, std::move(callback));
}

void FakeBluetoothAgentServiceProvider::AuthorizeService(
    const dbus::ObjectPath& device_path,
    const std::string& uuid,
    Delegate::ConfirmationCallback callback) {
  VLOG(1) << object_path_.value() << ": AuthorizeService " << uuid << " for "
          << device_path.value();
  delegate_->AuthorizeService(device_path, uuid, std::move(callback));
}

void FakeBluetoothAgentServiceProvider::Cancel() {
  VLOG(1) << object_path_.value() << ": Cancel";
  delegate_->Cancel();
}

}  // namespace bluez
