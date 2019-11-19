// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_discovery.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/fido/ble/fido_ble_device.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device_authenticator.h"

namespace device {

FidoBleDiscovery::FidoBleDiscovery()
    : FidoBleDiscoveryBase(FidoTransportProtocol::kBluetoothLowEnergy) {}

FidoBleDiscovery::~FidoBleDiscovery() = default;

// static
const BluetoothUUID& FidoBleDiscovery::FidoServiceUUID() {
  static const BluetoothUUID service_uuid(kFidoServiceUUID);
  return service_uuid;
}

void FidoBleDiscovery::OnSetPowered() {
  DCHECK(adapter());
  FIDO_LOG(DEBUG) << "Adapter " << adapter()->GetAddress() << " is powered on.";

  for (BluetoothDevice* device : adapter()->GetDevices()) {
    if (!CheckForExcludedDeviceAndCacheAddress(device) &&
        base::Contains(device->GetUUIDs(), FidoServiceUUID())) {
      const auto& device_address = device->GetAddress();
      FIDO_LOG(DEBUG) << "FIDO BLE device: " << device_address;
      AddDevice(std::make_unique<FidoBleDevice>(adapter(), device_address));
      CheckAndRecordDevicePairingModeOnDiscovery(
          FidoBleDevice::GetIdForAddress(device_address));
    }
  }

  auto discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
      BluetoothTransport::BLUETOOTH_TRANSPORT_LE);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(FidoServiceUUID());
  discovery_filter->AddDeviceFilter(device_filter);

  adapter()->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoBleDiscovery::OnStartDiscoverySessionWithFilter,
                         weak_factory_.GetWeakPtr())),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoBleDiscovery::OnStartDiscoverySessionError,
                         weak_factory_.GetWeakPtr())));
}

void FidoBleDiscovery::DeviceAdded(BluetoothAdapter* adapter,
                                   BluetoothDevice* device) {
  if (!CheckForExcludedDeviceAndCacheAddress(device) &&
      base::Contains(device->GetUUIDs(), FidoServiceUUID())) {
    const auto& device_address = device->GetAddress();
    FIDO_LOG(DEBUG) << "Discovered FIDO BLE device: " << device_address;
    AddDevice(std::make_unique<FidoBleDevice>(adapter, device_address));
    CheckAndRecordDevicePairingModeOnDiscovery(
        FidoBleDevice::GetIdForAddress(device_address));
  }
}

void FidoBleDiscovery::DeviceChanged(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (CheckForExcludedDeviceAndCacheAddress(device) ||
      !base::Contains(device->GetUUIDs(), FidoServiceUUID())) {
    return;
  }

  auto authenticator_id = FidoBleDevice::GetIdForAddress(device->GetAddress());
  auto* authenticator = GetAuthenticator(authenticator_id);
  if (!authenticator) {
    FIDO_LOG(DEBUG) << "Discovered FIDO service on existing BLE device: "
                    << device->GetAddress();
    AddDevice(std::make_unique<FidoBleDevice>(adapter, device->GetAddress()));
    CheckAndRecordDevicePairingModeOnDiscovery(std::move(authenticator_id));
    return;
  }

  if (authenticator->device()->IsInPairingMode()) {
    RecordDevicePairingStatus(std::move(authenticator_id),
                              PairingModeChangeType::kUnobserved);
  }
}

void FidoBleDiscovery::DeviceRemoved(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (base::Contains(device->GetUUIDs(), FidoServiceUUID())) {
    FIDO_LOG(DEBUG) << "FIDO BLE device removed: " << device->GetAddress();
    auto device_id = FidoBleDevice::GetIdForAddress(device->GetAddress());
    RemoveDevice(device_id);
    RemoveDeviceFromPairingTracker(device_id);
  }
}

void FidoBleDiscovery::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                             bool powered) {
  // If Bluetooth adapter is powered on, resume scanning for nearby FIDO
  // devices. Previously inactive discovery sessions would be terminated upon
  // invocation of OnSetPowered().
  if (powered)
    OnSetPowered();
}

void FidoBleDiscovery::DeviceAddressChanged(BluetoothAdapter* adapter,
                                            BluetoothDevice* device,
                                            const std::string& old_address) {
  auto previous_device_id = FidoBleDevice::GetIdForAddress(old_address);
  auto new_device_id = FidoBleDevice::GetIdForAddress(device->GetAddress());
  auto it = authenticators_.find(previous_device_id);
  if (it == authenticators_.end())
    return;

  it = authenticators_.find(new_device_id);
  // Don't proceed if new_device_id is already in the map, which indicates
  // a collision in addresses.
  if (it != authenticators_.end())
    return;

  FIDO_LOG(DEBUG)
      << "Discovered FIDO BLE device address change from old address : "
      << old_address << " to new address : " << device->GetAddress();

  auto change_map_keys = [&](auto* map) {
    auto it = map->find(previous_device_id);
    if (it != map->end()) {
      map->emplace(new_device_id, std::move(it->second));
      map->erase(it);
    }
  };

  change_map_keys(&authenticators_);
  change_map_keys(&pairing_mode_device_tracker_);

  if (observer()) {
    observer()->AuthenticatorIdChanged(this, previous_device_id,
                                       std::move(new_device_id));
  }
}

bool FidoBleDiscovery::CheckForExcludedDeviceAndCacheAddress(
    const BluetoothDevice* device) {
  std::string device_address = device->GetAddress();
  auto address_position =
      excluded_cable_device_addresses_.lower_bound(device_address);
  if (address_position != excluded_cable_device_addresses_.end() &&
      *address_position == device_address) {
    return true;
  }

  // IsCableDevice() is not stable, and can change throughout the lifetime. As
  // so, cache device address for known Cable devices so that we do not attempt
  // to connect to these devices.
  if (IsCableDevice(device)) {
    excluded_cable_device_addresses_.insert(address_position,
                                            std::move(device_address));
    return true;
  }

  return false;
}

void FidoBleDiscovery::CheckAndRecordDevicePairingModeOnDiscovery(
    std::string authenticator_id) {
  auto* authenticator = GetAuthenticator(authenticator_id);
  DCHECK(authenticator);
  if (authenticator->device()->IsInPairingMode()) {
    RecordDevicePairingStatus(std::move(authenticator_id),
                              PairingModeChangeType::kObserved);
  }
}

void FidoBleDiscovery::RecordDevicePairingStatus(std::string device_id,
                                                 PairingModeChangeType type) {
  auto it = pairing_mode_device_tracker_.find(device_id);
  if (it != pairing_mode_device_tracker_.end()) {
    it->second->Reset();
    return;
  }

  if (observer() && type == PairingModeChangeType::kUnobserved) {
    observer()->AuthenticatorPairingModeChanged(this, device_id,
                                                true /* is_in_pairing_mode */);
  }

  auto pairing_mode_timer = std::make_unique<base::OneShotTimer>();
  pairing_mode_timer->Start(
      FROM_HERE, kBleDevicePairingModeWaitingInterval,
      base::BindOnce(&FidoBleDiscovery::RemoveDeviceFromPairingTracker,
                     weak_factory_.GetWeakPtr(), device_id));
  pairing_mode_device_tracker_.emplace(std::move(device_id),
                                       std::move(pairing_mode_timer));
}

void FidoBleDiscovery::RemoveDeviceFromPairingTracker(
    const std::string& device_id) {
  // Destroying the timer stops the timer scheduled task.
  pairing_mode_device_tracker_.erase(device_id);
  if (observer()) {
    observer()->AuthenticatorPairingModeChanged(this, device_id,
                                                false /* is_in_pairing_mode */);
  }
}

}  // namespace device
