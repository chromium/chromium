// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_MAP_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_MAP_H_

#include <map>

#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

class BluetoothAllowedDevices;

// Class for keeping track of which origins are allowed to access which
// Bluetooth devices and their services.
class CONTENT_EXPORT BluetoothAllowedDevicesMap {
 public:
  BluetoothAllowedDevicesMap();

  BluetoothAllowedDevicesMap(const BluetoothAllowedDevicesMap&) = delete;
  BluetoothAllowedDevicesMap& operator=(const BluetoothAllowedDevicesMap&) =
      delete;

  ~BluetoothAllowedDevicesMap();

  // Gets a BluetoothAllowedDevices for each origin; creates one if it doesn't
  // exist.
  content::BluetoothAllowedDevices& GetOrCreateAllowedDevices(
      const url::Origin& origin);

  // Clears the data in |origin_to_allowed_devices_map_|.
  void Clear();

 private:
  std::map<url::Origin, content::BluetoothAllowedDevices>
      origin_to_allowed_devices_map_;
};

}  //  namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_MAP_H_
