// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_FILTER_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_FILTER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

// *****************************************************************************
// BluetoothDiscoveryFilter is a class which stores information used to filter
// out Bluetooth devices at the operating system level while doing discovery.
// If you want to filter by RSSI or path loss set them directly in the class
// with the SetRSSI() and SetPathloss() functions.  However, if you are looking
// for a device with a particular name and/or set of services you must add a
// DeviceInfoFilter.
// Here is an example usage for DeviceInfoFilters:
//
// BluetoothDiscoveryFilter discovery_filter(BLUETOOTH_TRANSPORT_LE);
// BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
// device_filter.uuids.insert(BluetoothUUID("1019"));
// device_filter.uuids.insert(BluetoothUUID("1020"));
// discovery_filter.AddDeviceFilter(device_filter);
//
// BluetoothDiscoveryFilter::DeviceInfoFilter device_filter2;
// device_filter2.uuids.insert(BluetoothUUID("1021"));
// device_filter2.name = "this device";
// discovery_filter.AddDeviceFilter(device_filter2);
//
// When we add |device_filter| to |discovery_filter| our filter will only return
// devices that have both the uuid 1019 AND 1020.  When we add |device_filter2|
// we will then allow devices though that have either (uuid 1019 AND 1020) OR
// (uuid 1021 and a device name of "this device").
// *****************************************************************************

class DEVICE_BLUETOOTH_EXPORT BluetoothDiscoveryFilter {
 public:
  BluetoothDiscoveryFilter();
  BluetoothDiscoveryFilter(BluetoothTransport transport);

  BluetoothDiscoveryFilter(const BluetoothDiscoveryFilter&) = delete;
  BluetoothDiscoveryFilter& operator=(const BluetoothDiscoveryFilter&) = delete;

  ~BluetoothDiscoveryFilter();

  struct DEVICE_BLUETOOTH_EXPORT DeviceInfoFilter {
    DeviceInfoFilter();
    DeviceInfoFilter(const DeviceInfoFilter& other);
    ~DeviceInfoFilter();
    bool operator==(const DeviceInfoFilter& other) const;
    bool operator<(const DeviceInfoFilter& other) const;
    base::flat_set<device::BluetoothUUID> uuids;
    std::string name;
  };

  // These getters return true when given field is set in filter, and copy this
  // value to |out_*| parameter. If value is not set, returns false.
  // These setters assign given value to proper filter field.
  bool GetRSSI(int16_t* out_rssi) const;
  void SetRSSI(int16_t rssi);
  bool GetPathloss(uint16_t* out_pathloss) const;
  void SetPathloss(uint16_t pathloss);

  // Return and set transport field of this filter.
  BluetoothTransport GetTransport() const;
  void SetTransport(BluetoothTransport transport);

  // Make |out_uuids| represent all uuids in the |device_filters_| set.
  void GetUUIDs(std::set<device::BluetoothUUID>& out_uuids) const;

  // Add new DeviceInfoFilter to our array of DeviceInfoFilters,
  void AddDeviceFilter(const DeviceInfoFilter& device_filter);

  // Returns a const pointer of our list of DeviceInfoFilters, device_filters_.
  const base::flat_set<DeviceInfoFilter>* GetDeviceFilters() const;

  // Copy content of |filter| and assigns it to this filter.
  void CopyFrom(const BluetoothDiscoveryFilter& filter);

  // Check if two filters are equal.
  bool Equals(const BluetoothDiscoveryFilter& filter) const;

  // Returns true if all fields in filter are empty
  bool IsDefault() const;

  void ClearDeviceFilters();

  // Returns result of merging two filters together. If at least one of the
  // filters is NULL this will return an empty filter
  static std::unique_ptr<device::BluetoothDiscoveryFilter> Merge(
      const device::BluetoothDiscoveryFilter* filter_a,
      const device::BluetoothDiscoveryFilter* filter_b);

 private:
  std::optional<int16_t> rssi_;
  std::optional<uint16_t> pathloss_;
  BluetoothTransport transport_;
  base::flat_set<DeviceInfoFilter> device_filters_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_FILTER_H_
