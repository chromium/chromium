// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"

#include <map>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

FakeBluetoothInputClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothInputClient::Properties(
          nullptr,
          bluetooth_input::kBluetoothInputInterface,
          callback) {}

FakeBluetoothInputClient::Properties::~Properties() = default;

void FakeBluetoothInputClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeBluetoothInputClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeBluetoothInputClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  std::move(callback).Run(false);
}

FakeBluetoothInputClient::FakeBluetoothInputClient() = default;

FakeBluetoothInputClient::~FakeBluetoothInputClient() = default;

void FakeBluetoothInputClient::Init(dbus::Bus* bus,
                                    const std::string& bluetooth_service_name) {
}

void FakeBluetoothInputClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothInputClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

FakeBluetoothInputClient::Properties* FakeBluetoothInputClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  auto iter = properties_map_.find(object_path);
  if (iter != properties_map_.end())
    return iter->second.get();
  return nullptr;
}

void FakeBluetoothInputClient::AddInputDevice(
    const dbus::ObjectPath& object_path) {
  if (properties_map_.find(object_path) != properties_map_.end())
    return;

  std::unique_ptr<Properties> properties = std::make_unique<Properties>(
      base::Bind(&FakeBluetoothInputClient::OnPropertyChanged,
                 base::Unretained(this), object_path));

  // The LegacyAutopair and DisplayPinCode devices represent a typical mouse
  // and keyboard respectively, so mark them as ReconnectMode "any". The
  // DisplayPasskey device represents a Bluetooth 2.1+ keyboard and the
  // ConnectUnpairable device represents a pre-standardization mouse, so mark
  // them as ReconnectMode "device".
  if (object_path.value() == FakeBluetoothDeviceClient::kDisplayPasskeyPath ||
      object_path.value() ==
          FakeBluetoothDeviceClient::kConnectUnpairablePath) {
    properties->reconnect_mode.ReplaceValue(
        bluetooth_input::kDeviceReconnectModeProperty);
  } else {
    properties->reconnect_mode.ReplaceValue(
        bluetooth_input::kAnyReconnectModeProperty);
  }

  properties_map_[object_path] = std::move(properties);

  for (auto& observer : observers_)
    observer.InputAdded(object_path);
}

void FakeBluetoothInputClient::RemoveInputDevice(
    const dbus::ObjectPath& object_path) {
  auto it = properties_map_.find(object_path);

  if (it == properties_map_.end())
    return;

  for (auto& observer : observers_)
    observer.InputRemoved(object_path);

  properties_map_.erase(it);
}

void FakeBluetoothInputClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  for (auto& observer : observers_)
    observer.InputPropertyChanged(object_path, property_name);
}

}  // namespace bluez
