// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_allowed_devices.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "content/browser/bluetooth/bluetooth_blocklist.h"

using device::BluetoothUUID;

namespace content {

BluetoothAllowedDevices::BluetoothAllowedDevices() {}
BluetoothAllowedDevices::BluetoothAllowedDevices(
    const BluetoothAllowedDevices& other) = default;
BluetoothAllowedDevices::~BluetoothAllowedDevices() {}

const blink::WebBluetoothDeviceId& BluetoothAllowedDevices::AddDevice(
    const std::string& device_address,
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  auto& device_id = AddDevice(device_address);
  AddUnionOfServicesTo(options, &device_id_to_services_map_[device_id]);

  // Currently, devices that are added with WebBluetoothRequestDeviceOptionsPtr
  // |options| come from RequestDevice() and therefore have the ablity to be
  // connected to.
  device_id_to_connectable_map_[device_id] = true;

  return device_id;
}

const blink::WebBluetoothDeviceId& BluetoothAllowedDevices::AddDevice(
    const std::string& device_address) {
  DVLOG(1) << "Adding a device to Map of Allowed Devices.";

  auto id_iter = device_address_to_id_map_.find(device_address);
  if (id_iter != device_address_to_id_map_.end()) {
    DVLOG(1) << "Device already in map of allowed devices.";
    return device_address_to_id_map_[device_address];
  }
  const blink::WebBluetoothDeviceId device_id = GenerateUniqueDeviceId();
  DVLOG(1) << "Id generated for device: " << device_id;

  device_address_to_id_map_[device_address] = device_id;
  device_id_to_address_map_[device_id] = device_address;

  CHECK(device_id_set_.insert(device_id).second);

  return device_address_to_id_map_[device_address];
}

void BluetoothAllowedDevices::RemoveDevice(const std::string& device_address) {
  const blink::WebBluetoothDeviceId* device_id_ptr =
      GetDeviceId(device_address);
  DCHECK(device_id_ptr != nullptr);

  // We make a copy because we are going to remove the original value from its
  // map.
  blink::WebBluetoothDeviceId device_id = *device_id_ptr;

  // 1. Remove from all three maps.
  CHECK(device_address_to_id_map_.erase(device_address));
  CHECK(device_id_to_address_map_.erase(device_id));
  CHECK(device_id_to_services_map_.erase(device_id));

  // Not all devices are connectable.
  device_id_to_connectable_map_.erase(device_id);

  // 2. Remove from set of ids.
  CHECK(device_id_set_.erase(device_id));
}

const blink::WebBluetoothDeviceId* BluetoothAllowedDevices::GetDeviceId(
    const std::string& device_address) {
  auto id_iter = device_address_to_id_map_.find(device_address);
  if (id_iter == device_address_to_id_map_.end()) {
    return nullptr;
  }
  return &(id_iter->second);
}

const std::string& BluetoothAllowedDevices::GetDeviceAddress(
    const blink::WebBluetoothDeviceId& device_id) {
  auto id_iter = device_id_to_address_map_.find(device_id);

  return id_iter == device_id_to_address_map_.end() ? base::EmptyString()
                                                    : id_iter->second;
}

bool BluetoothAllowedDevices::IsAllowedToAccessAtLeastOneService(
    const blink::WebBluetoothDeviceId& device_id) const {
  auto id_iter = device_id_to_services_map_.find(device_id);

  return id_iter == device_id_to_services_map_.end() ? false
                                                     : !id_iter->second.empty();
}

bool BluetoothAllowedDevices::IsAllowedToAccessService(
    const blink::WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service_uuid) const {
  if (BluetoothBlocklist::Get().IsExcluded(service_uuid)) {
    return false;
  }

  auto id_iter = device_id_to_services_map_.find(device_id);

  return id_iter == device_id_to_services_map_.end()
             ? false
             : base::Contains(id_iter->second, service_uuid);
}

bool BluetoothAllowedDevices::IsAllowedToGATTConnect(
    const blink::WebBluetoothDeviceId& device_id) const {
  auto id_iter = device_id_to_connectable_map_.find(device_id);
  if (id_iter == device_id_to_connectable_map_.end())
    return false;
  return id_iter->second;
}

blink::WebBluetoothDeviceId BluetoothAllowedDevices::GenerateUniqueDeviceId() {
  blink::WebBluetoothDeviceId device_id = blink::WebBluetoothDeviceId::Create();
  while (base::Contains(device_id_set_, device_id)) {
    LOG(WARNING) << "Generated repeated id.";
    device_id = blink::WebBluetoothDeviceId::Create();
  }
  return device_id;
}

void BluetoothAllowedDevices::AddUnionOfServicesTo(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options,
    std::unordered_set<BluetoothUUID, device::BluetoothUUIDHash>*
        unionOfServices) {
  if (options->filters) {
    for (const auto& filter : options->filters.value()) {
      if (!filter->services) {
        continue;
      }

      for (const BluetoothUUID& uuid : filter->services.value()) {
        unionOfServices->insert(uuid);
      }
    }
  }

  for (const BluetoothUUID& uuid : options->optional_services) {
    unionOfServices->insert(uuid);
  }
}

}  // namespace content
