// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/bluetooth/bluetooth_system.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace device {

void BluetoothSystem::Create(
    mojo::PendingReceiver<mojom::BluetoothSystem> receiver,
    mojo::PendingRemote<mojom::BluetoothSystemClient> client) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<BluetoothSystem>(std::move(client)),
      std::move(receiver));
}

BluetoothSystem::BluetoothSystem(
    mojo::PendingRemote<mojom::BluetoothSystemClient> client)
    : client_(std::move(client)) {
  GetBluetoothAdapterClient()->AddObserver(this);

  std::vector<dbus::ObjectPath> object_paths =
      GetBluetoothAdapterClient()->GetAdapters();
  if (object_paths.empty())
    return;

  active_adapter_ = object_paths[0];
  auto* properties =
      GetBluetoothAdapterClient()->GetProperties(active_adapter_.value());
  state_ = properties->powered.value() ? State::kPoweredOn : State::kPoweredOff;
}

BluetoothSystem::~BluetoothSystem() = default;

void BluetoothSystem::AdapterAdded(const dbus::ObjectPath& object_path) {
  if (active_adapter_)
    return;

  active_adapter_ = object_path;
  UpdateStateAndNotifyIfNecessary();
}

void BluetoothSystem::AdapterRemoved(const dbus::ObjectPath& object_path) {
  DCHECK(active_adapter_);

  if (active_adapter_.value() != object_path)
    return;

  active_adapter_ = base::nullopt;

  std::vector<dbus::ObjectPath> object_paths =
      GetBluetoothAdapterClient()->GetAdapters();
  for (const auto& new_object_path : object_paths) {
    // The removed adapter is still included in GetAdapters().
    if (new_object_path == object_path)
      continue;

    active_adapter_ = new_object_path;
    break;
  }

  UpdateStateAndNotifyIfNecessary();
}

void BluetoothSystem::AdapterPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  // AdapterPropertyChanged is called for each property in the adapter before
  // AdapterAdded is called. Immediately return in this case.
  if (!active_adapter_)
    return;

  if (active_adapter_.value() != object_path)
    return;

  auto* properties =
      GetBluetoothAdapterClient()->GetProperties(active_adapter_.value());

  if (properties->powered.name() == property_name)
    UpdateStateAndNotifyIfNecessary();
  else if (properties->discovering.name() == property_name)
    client_->OnScanStateChanged(GetScanStateFromActiveAdapter());
}

void BluetoothSystem::GetState(GetStateCallback callback) {
  std::move(callback).Run(state_);
}

void BluetoothSystem::SetPowered(bool powered, SetPoweredCallback callback) {
  switch (state_) {
    case State::kUnsupported:
    case State::kUnavailable:
      std::move(callback).Run(SetPoweredResult::kFailedBluetoothUnavailable);
      return;
    case State::kTransitioning:
      std::move(callback).Run(SetPoweredResult::kFailedInProgress);
      return;
    case State::kPoweredOff:
    case State::kPoweredOn:
      break;
  }

  if ((state_ == State::kPoweredOn) == powered) {
    std::move(callback).Run(SetPoweredResult::kSuccess);
    return;
  }

  DCHECK_NE(state_, State::kTransitioning);
  state_ = State::kTransitioning;
  client_->OnStateChanged(state_);

  GetBluetoothAdapterClient()
      ->GetProperties(active_adapter_.value())
      ->powered.Set(powered,
                    base::BindRepeating(&BluetoothSystem::OnSetPoweredFinished,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        base::Passed(&callback)));
}

void BluetoothSystem::GetScanState(GetScanStateCallback callback) {
  switch (state_) {
    case State::kUnsupported:
    case State::kUnavailable:
      std::move(callback).Run(ScanState::kNotScanning);
      return;
    case State::kPoweredOff:
    // The Scan State when the adapter is Off should always be
    // kNotScanning, but the underlying layer makes no guarantees of this.
    // To avoid hiding a bug in the underlying layer, get the state from
    // the adapter even if it's Off.
    case State::kTransitioning:
    case State::kPoweredOn:
      break;
  }

  std::move(callback).Run(GetScanStateFromActiveAdapter());
}

void BluetoothSystem::StartScan(StartScanCallback callback) {
  switch (state_) {
    case State::kUnsupported:
    case State::kUnavailable:
    case State::kPoweredOff:
    case State::kTransitioning:
      std::move(callback).Run(StartScanResult::kFailedBluetoothUnavailable);
      return;
    case State::kPoweredOn:
      break;
  }

  GetBluetoothAdapterClient()->StartDiscovery(
      active_adapter_.value(),
      base::BindOnce(&BluetoothSystem::OnStartDiscovery,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BluetoothSystem::StopScan(StopScanCallback callback) {
  switch (state_) {
    case State::kUnsupported:
    case State::kUnavailable:
    case State::kPoweredOff:
    case State::kTransitioning:
      std::move(callback).Run(StopScanResult::kFailedBluetoothUnavailable);
      return;
    case State::kPoweredOn:
      break;
  }

  GetBluetoothAdapterClient()->StopDiscovery(
      active_adapter_.value(),
      base::BindOnce(&BluetoothSystem::OnStopDiscovery,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BluetoothSystem::GetAvailableDevices(
    GetAvailableDevicesCallback callback) {
  switch (state_) {
    case State::kUnsupported:
    case State::kUnavailable:
    case State::kPoweredOff:
    case State::kTransitioning:
      std::move(callback).Run({});
      return;
    case State::kPoweredOn:
      break;
  }

  std::vector<dbus::ObjectPath> device_paths =
      GetBluetoothDeviceClient()->GetDevicesForAdapter(active_adapter_.value());

  std::vector<mojom::BluetoothDeviceInfoPtr> devices;
  for (const auto& device_path : device_paths) {
    auto* properties = GetBluetoothDeviceClient()->GetProperties(device_path);
    std::array<uint8_t, 6> parsed_address;
    if (!BluetoothDevice::ParseAddress(properties->address.value(),
                                       parsed_address)) {
      LOG(WARNING) << "Failed to parse device address '"
                   << properties->address.value() << "' for "
                   << device_path.value();
      continue;
    }

    auto device_info = mojom::BluetoothDeviceInfo::New();
    device_info->address = std::move(parsed_address);
    device_info->name = properties->name.is_valid()
                            ? base::make_optional(properties->name.value())
                            : base::nullopt;
    device_info->connection_state =
        properties->connected.value()
            ? mojom::BluetoothDeviceInfo::ConnectionState::kConnected
            : mojom::BluetoothDeviceInfo::ConnectionState::kNotConnected;
    device_info->is_paired = properties->paired.value();

    // TODO(ortuno): Get the DeviceType from the device Class and Appearance.
    devices.push_back(std::move(device_info));
  }
  std::move(callback).Run(std::move(devices));
}

bluez::BluetoothAdapterClient* BluetoothSystem::GetBluetoothAdapterClient() {
  // Use AlternateBluetoothAdapterClient to avoid interfering with users of the
  // regular BluetoothAdapterClient.
  return bluez::BluezDBusManager::Get()->GetAlternateBluetoothAdapterClient();
}

bluez::BluetoothDeviceClient* BluetoothSystem::GetBluetoothDeviceClient() {
  // Use AlternateBluetoothDeviceClient to avoid interfering with users of the
  // regular BluetoothDeviceClient.
  return bluez::BluezDBusManager::Get()->GetAlternateBluetoothDeviceClient();
}

void BluetoothSystem::UpdateStateAndNotifyIfNecessary() {
  State old_state = state_;
  if (active_adapter_) {
    auto* properties =
        GetBluetoothAdapterClient()->GetProperties(active_adapter_.value());
    state_ =
        properties->powered.value() ? State::kPoweredOn : State::kPoweredOff;
  } else {
    state_ = State::kUnavailable;
  }

  if (old_state != state_)
    client_->OnStateChanged(state_);
}

BluetoothSystem::ScanState BluetoothSystem::GetScanStateFromActiveAdapter() {
  bool discovering = GetBluetoothAdapterClient()
                         ->GetProperties(active_adapter_.value())
                         ->discovering.value();
  return discovering ? ScanState::kScanning : ScanState::kNotScanning;
}

void BluetoothSystem::OnSetPoweredFinished(SetPoweredCallback callback,
                                           bool succeeded) {
  if (!succeeded) {
    // We change |state_| to `kTransitioning` before trying to set 'powered'. If
    // the call to set 'powered' fails, then we need to change it back to
    // `kPoweredOn` or `kPoweredOff` depending on the `active_adapter_` state.
    UpdateStateAndNotifyIfNecessary();
  }

  std::move(callback).Run(succeeded ? SetPoweredResult::kSuccess
                                    : SetPoweredResult::kFailedUnknownReason);
}

void BluetoothSystem::OnStartDiscovery(
    StartScanCallback callback,
    const base::Optional<bluez::BluetoothAdapterClient::Error>& error) {
  // TODO(https://crbug.com/897996): Use the name and message in |error| to
  // return more specific error codes.
  std::move(callback).Run(error ? StartScanResult::kFailedUnknownReason
                                : StartScanResult::kSuccess);
}

void BluetoothSystem::OnStopDiscovery(
    StopScanCallback callback,
    const base::Optional<bluez::BluetoothAdapterClient::Error>& error) {
  // TODO(https://crbug.com/897996): Use the name and message in |error| to
  // return more specific error codes.
  std::move(callback).Run(error ? StopScanResult::kFailedUnknownReason
                                : StopScanResult::kSuccess);
}

}  // namespace device
