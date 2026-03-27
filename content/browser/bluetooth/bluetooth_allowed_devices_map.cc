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
  return origin_to_allowed_devices_map_.try_emplace(origin).first->second;
}

void BluetoothAllowedDevicesMap::Clear() {
  origin_to_allowed_devices_map_.clear();
}

}  //  namespace content
