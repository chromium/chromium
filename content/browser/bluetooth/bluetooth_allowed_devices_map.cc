// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"

#include <utility>

#include "content/browser/bluetooth/bluetooth_allowed_devices.h"

namespace content {

BluetoothAllowedDevicesMap::BluetoothAllowedDevicesMap() = default;

BluetoothAllowedDevicesMap::~BluetoothAllowedDevicesMap() = default;

content::BluetoothAllowedDevices&
BluetoothAllowedDevicesMap::GetOrCreateAllowedDevices(
    const url::Origin& origin) {
  // "Unique" Origins generate the same key in maps, therefore are not
  // supported.
  CHECK(!origin.opaque()) << " origin: " << origin;
  auto iter = origin_to_allowed_devices_map_.find(origin);
  if (iter == origin_to_allowed_devices_map_.end()) {
    iter = origin_to_allowed_devices_map_.insert(
        iter, std::make_pair(origin, content::BluetoothAllowedDevices()));
  }
  return iter->second;
}

void BluetoothAllowedDevicesMap::Clear() {
  origin_to_allowed_devices_map_.clear();
}

}  //  namespace content
