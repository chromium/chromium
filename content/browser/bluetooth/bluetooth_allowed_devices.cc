// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_allowed_devices.h"

#include <string>

#include "base/containers/map_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/browser/bluetooth/bluetooth_blocklist.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

using device::BluetoothUUID;

namespace content {

BluetoothAllowedDevices::BluetoothAllowedDevices() = default;

BluetoothAllowedDevices::BluetoothAllowedDevices(BluetoothAllowedDevices&&) =
    default;

BluetoothAllowedDevices& BluetoothAllowedDevices::operator=(
    BluetoothAllowedDevices&&) = default;

BluetoothAllowedDevices::~BluetoothAllowedDevices() = default;

BluetoothAllowedDevices::DeviceMetadata::DeviceMetadata(
    const std::string& device_address)
    : device_address(device_address) {}

BluetoothAllowedDevices::DeviceMetadata::DeviceMetadata(DeviceMetadata&&) =
    default;

BluetoothAllowedDevices::DeviceMetadata&
BluetoothAllowedDevices::DeviceMetadata::operator=(DeviceMetadata&&) = default;

BluetoothAllowedDevices::DeviceMetadata::~DeviceMetadata() = default;

blink::WebBluetoothDeviceId BluetoothAllowedDevices::AddDevice(
    const std::string& device_address,
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  auto [device_id, metadata] = AddDeviceInternal(device_address);

  if (options->filters) {
    for (const auto& filter : options->filters.value()) {
      if (!filter->services) {
        continue;
      }

      metadata.allowed_services.insert_range(filter->services.value());
    }
  }

  metadata.allowed_services.insert_range(options->optional_services);

  metadata.allowed_manufacturers.insert_range(
      options->optional_manufacturer_data);

  // Currently, devices that are added with WebBluetoothRequestDeviceOptionsPtr
  // |options| come from RequestDevice() and therefore have the ablity to be
  // connected to.
  metadata.is_connectable = true;

  return device_id;
}

blink::WebBluetoothDeviceId BluetoothAllowedDevices::AddDevice(
    const std::string& device_address) {
  return AddDeviceInternal(device_address).first;
}

BluetoothAllowedDevices::AddDeviceResult
BluetoothAllowedDevices::AddDeviceInternal(const std::string& device_address) {
  DVLOG(1) << "Adding a device to Map of Allowed Devices.";

  auto [id_iter, inserted] =
      device_address_to_id_map_.try_emplace(device_address);
  if (!inserted) {
    DVLOG(1) << "Device already in map of allowed devices.";
    return {id_iter->second, device_id_to_metadata_map_.at(id_iter->second)};
  }

  while (true) {
    blink::WebBluetoothDeviceId device_id =
        blink::WebBluetoothDeviceId::Create();
    auto [metadata_iter, metadata_inserted] =
        device_id_to_metadata_map_.try_emplace(device_id, device_address);
    if (metadata_inserted) {
      id_iter->second = device_id;
      DVLOG(1) << "Id generated for device: " << device_id;
      return {id_iter->second, metadata_iter->second};
    }
    LOG(WARNING) << "Generated repeated id.";
  }
}

void BluetoothAllowedDevices::RemoveDevice(const std::string& device_address) {
  auto it = device_address_to_id_map_.find(device_address);
  CHECK(it != device_address_to_id_map_.end());

  // Remove from both maps.
  CHECK(device_id_to_metadata_map_.erase(it->second));
  device_address_to_id_map_.erase(it);
}

const blink::WebBluetoothDeviceId* BluetoothAllowedDevices::GetDeviceId(
    const std::string& device_address) const {
  return base::FindOrNull(device_address_to_id_map_, device_address);
}

const std::string& BluetoothAllowedDevices::GetDeviceAddress(
    const blink::WebBluetoothDeviceId& device_id) const {
  auto it = device_id_to_metadata_map_.find(device_id);
  return it == device_id_to_metadata_map_.end() ? base::EmptyString()
                                                : it->second.device_address;
}

bool BluetoothAllowedDevices::IsAllowedToAccessAtLeastOneService(
    const blink::WebBluetoothDeviceId& device_id) const {
  auto it = device_id_to_metadata_map_.find(device_id);
  return it == device_id_to_metadata_map_.end()
             ? false
             : !it->second.allowed_services.empty();
}

bool BluetoothAllowedDevices::IsAllowedToAccessService(
    const blink::WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service_uuid) const {
  if (BluetoothBlocklist::Get().IsExcluded(service_uuid))
    return false;

  auto it = device_id_to_metadata_map_.find(device_id);
  return it == device_id_to_metadata_map_.end()
             ? false
             : it->second.allowed_services.contains(service_uuid);
}

bool BluetoothAllowedDevices::IsAllowedToGATTConnect(
    const blink::WebBluetoothDeviceId& device_id) const {
  auto it = device_id_to_metadata_map_.find(device_id);
  return it == device_id_to_metadata_map_.end() ? false
                                                : it->second.is_connectable;
}

bool BluetoothAllowedDevices::IsAllowedToAccessManufacturerData(
    const blink::WebBluetoothDeviceId& device_id,
    const uint16_t manufacturer_code) const {
  auto it = device_id_to_metadata_map_.find(device_id);
  return it == device_id_to_metadata_map_.end()
             ? false
             : it->second.allowed_manufacturers.contains(manufacturer_code);
}

}  // namespace content
