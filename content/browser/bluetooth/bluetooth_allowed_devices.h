// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/optional.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace device {
class BluetoothUUID;
}

namespace content {

// Tracks the devices and their services that a site has been granted
// access to.
//
// |AddDevice| generates device ids, which are random strings that are unique
// in the map.
class CONTENT_EXPORT BluetoothAllowedDevices final {
 public:
  BluetoothAllowedDevices();
  BluetoothAllowedDevices(const BluetoothAllowedDevices& other);
  ~BluetoothAllowedDevices();

  // Adds the Bluetooth Device with |device_address| to the map of allowed
  // devices. Generates and returns a new random device ID so that devices
  // IDs can not be compared between sites.
  const blink::WebBluetoothDeviceId& AddDevice(
      const std::string& device_address,
      const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options);

  // Same as the above version of |AddDevice| but does not add any services to
  // the device id -> services map.
  const blink::WebBluetoothDeviceId& AddDevice(
      const std::string& device_address);

  // Removes the Bluetooth Device with |device_address| from the map of allowed
  // devices.
  void RemoveDevice(const std::string& device_address);

  // Returns the Bluetooth Device's id if it has been added previously with
  // |AddDevice|.
  const blink::WebBluetoothDeviceId* GetDeviceId(
      const std::string& device_address);

  // For |device_id|, returns the Bluetooth device's address. If there is no
  // such |device_id|, returns an empty string.
  const std::string& GetDeviceAddress(
      const blink::WebBluetoothDeviceId& device_id);

  // Returns true if access has previously been granted to at least one
  // service.
  bool IsAllowedToAccessAtLeastOneService(
      const blink::WebBluetoothDeviceId& device_id) const;

  // Returns true if access has previously been granted to the service.
  bool IsAllowedToAccessService(
      const blink::WebBluetoothDeviceId& device_id,
      const device::BluetoothUUID& service_uuid) const;

  bool IsAllowedToGATTConnect(
      const blink::WebBluetoothDeviceId& device_id) const;

 private:
  typedef std::unordered_map<std::string, blink::WebBluetoothDeviceId>
      DeviceAddressToIdMap;
  typedef std::unordered_map<blink::WebBluetoothDeviceId,
                             std::string,
                             blink::WebBluetoothDeviceIdHash>
      DeviceIdToAddressMap;
  typedef std::unordered_map<
      blink::WebBluetoothDeviceId,
      std::unordered_set<device::BluetoothUUID, device::BluetoothUUIDHash>,
      blink::WebBluetoothDeviceIdHash>
      DeviceIdToServicesMap;
  typedef std::unordered_map<blink::WebBluetoothDeviceId,
                             bool,
                             blink::WebBluetoothDeviceIdHash>
      DeviceIdToConnectableMap;

  // Returns an id guaranteed to be unique for the map. The id is randomly
  // generated so that an origin can't guess the id used in another origin.
  blink::WebBluetoothDeviceId GenerateUniqueDeviceId();
  void AddUnionOfServicesTo(
      const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options,
      std::unordered_set<device::BluetoothUUID, device::BluetoothUUIDHash>*
          unionOfServices);

  DeviceAddressToIdMap device_address_to_id_map_;
  DeviceIdToAddressMap device_id_to_address_map_;
  DeviceIdToServicesMap device_id_to_services_map_;
  DeviceIdToConnectableMap device_id_to_connectable_map_;

  // Keep track of all device_ids in the map.
  std::unordered_set<blink::WebBluetoothDeviceId,
                     blink::WebBluetoothDeviceIdHash>
      device_id_set_;
};

}  //  namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_H_
