// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_LE_ADVERTISEMENT_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_LE_ADVERTISEMENT_SERVICE_PROVIDER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace bluez {

// BluetoothAdvertisementServiceProvider is used to provide a D-Bus object that
// the Bluetooth daemon can communicate with to advertise data.
class DEVICE_BLUETOOTH_EXPORT BluetoothLEAdvertisementServiceProvider {
 public:
  using UUIDList = std::vector<std::string>;
  using ManufacturerData = std::map<uint16_t, std::vector<uint8_t>>;
  using ServiceData = std::map<std::string, std::vector<uint8_t>>;
  using ScanResponseData = std::map<uint8_t, std::vector<uint8_t>>;

  // Type of advertisement.
  enum AdvertisementType {
    ADVERTISEMENT_TYPE_BROADCAST,
    ADVERTISEMENT_TYPE_PERIPHERAL
  };

  // Interface for reacting to advertisement changes.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // This method will be called when the advertisement is unregistered from
    // the Bluetooth daemon, generally at shutdown or if the adapter goes away.
    // It may be used to perform cleanup tasks. This corresponds to the
    // org.bluez.LEAdvertisement1.Release method and is renamed to avoid a
    // conflict with base::Refcounted<T>.
    virtual void Released() = 0;
  };

  BluetoothLEAdvertisementServiceProvider(
      const BluetoothLEAdvertisementServiceProvider&) = delete;
  BluetoothLEAdvertisementServiceProvider& operator=(
      const BluetoothLEAdvertisementServiceProvider&) = delete;

  virtual ~BluetoothLEAdvertisementServiceProvider();

  const dbus::ObjectPath& object_path() { return object_path_; }

  // Creates the instance where |bus| is the D-Bus bus connection to export
  // the object onto, |object_path| is the object path that it should have
  // and |delegate| is the object to which all method calls will be passed
  // and responses generated from.
  static std::unique_ptr<BluetoothLEAdvertisementServiceProvider> Create(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      Delegate* delegate,
      AdvertisementType type,
      absl::optional<UUIDList> service_uuids,
      absl::optional<ManufacturerData> manufacturer_data,
      absl::optional<UUIDList> solicit_uuids,
      absl::optional<ServiceData> service_data,
      absl::optional<ScanResponseData> scan_response_data);

 protected:
  BluetoothLEAdvertisementServiceProvider();

  // D-Bus object path of object we are exporting, kept so we can unregister
  // again in our destructor.
  dbus::ObjectPath object_path_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_LE_ADVERTISEMENT_SERVICE_PROVIDER_H_
