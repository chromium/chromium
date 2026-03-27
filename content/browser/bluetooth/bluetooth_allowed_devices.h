// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_H_

#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "content/common/content_export.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-forward.h"

namespace content {

// Tracks the devices and their services that a site has been granted
// access to.
//
// |AddDevice| generates device ids, which are random strings that are unique
// in the map.
class CONTENT_EXPORT BluetoothAllowedDevices final {
 public:
  BluetoothAllowedDevices();
  BluetoothAllowedDevices(const BluetoothAllowedDevices&) = delete;
  BluetoothAllowedDevices& operator=(const BluetoothAllowedDevices&) = delete;
  BluetoothAllowedDevices(BluetoothAllowedDevices&&);
  BluetoothAllowedDevices& operator=(BluetoothAllowedDevices&&);
  ~BluetoothAllowedDevices();

  // Adds the Bluetooth Device with |device_address| to the map of allowed
  // devices. Generates and returns a new random device ID so that devices
  // IDs can not be compared between sites.
  blink::WebBluetoothDeviceId AddDevice(
      const std::string& device_address,
      const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options);

  // Same as the above version of |AddDevice| but does not add any services to
  // the device id -> services map.
  blink::WebBluetoothDeviceId AddDevice(const std::string& device_address);

  // Removes the Bluetooth Device with |device_address| from the map of allowed
  // devices.
  void RemoveDevice(const std::string& device_address);

  // Returns the Bluetooth Device's id if it has been added previously with
  // |AddDevice|.
  const blink::WebBluetoothDeviceId* GetDeviceId(
      const std::string& device_address) const;

  // For |device_id|, returns the Bluetooth device's address. If there is no
  // such |device_id|, returns an empty string.
  const std::string& GetDeviceAddress(
      const blink::WebBluetoothDeviceId& device_id) const;

  // Returns true if access has previously been granted to at least one
  // service.
  bool IsAllowedToAccessAtLeastOneService(
      const blink::WebBluetoothDeviceId& device_id) const;

  // Returns true if access has previously been granted to the service.
  bool IsAllowedToAccessService(
      const blink::WebBluetoothDeviceId& device_id,
      const device::BluetoothUUID& service_uuid) const;

  // Returns true if access has been previously granted for manufacturer data
  // corresponding to |manufacturer_code|.
  bool IsAllowedToAccessManufacturerData(
      const blink::WebBluetoothDeviceId& device_id,
      const uint16_t manufacturer_code) const;

  bool IsAllowedToGATTConnect(
      const blink::WebBluetoothDeviceId& device_id) const;

 private:
  struct DeviceMetadata {
    explicit DeviceMetadata(const std::string& device_address);
    DeviceMetadata(const DeviceMetadata&) = delete;
    DeviceMetadata& operator=(const DeviceMetadata&) = delete;
    DeviceMetadata(DeviceMetadata&&);
    DeviceMetadata& operator=(DeviceMetadata&&);
    ~DeviceMetadata();

    std::string device_address;
    base::flat_set<device::BluetoothUUID> allowed_services;
    base::flat_set<uint16_t> allowed_manufacturers;
    bool is_connectable = false;
  };

  using AddDeviceResult =
      std::pair<blink::WebBluetoothDeviceId, DeviceMetadata&>;

  using DeviceAddressToIdMap =
      absl::flat_hash_map<std::string, blink::WebBluetoothDeviceId>;

  using DeviceIdToMetadataMap =
      absl::flat_hash_map<blink::WebBluetoothDeviceId,
                          DeviceMetadata,
                          blink::WebBluetoothDeviceIdHash>;

  AddDeviceResult AddDeviceInternal(const std::string& device_address);

  DeviceAddressToIdMap device_address_to_id_map_;
  DeviceIdToMetadataMap device_id_to_metadata_map_;
};

}  //  namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_H_
