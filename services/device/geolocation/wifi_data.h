// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_H_

#include <set>
#include <string>

#include "services/device/public/mojom/geolocation_internals.mojom.h"

namespace device {

// This is to allow AccessPointData to be used in std::set. We order
// lexicographically by MAC address.
struct AccessPointDataLess {
  bool operator()(const mojom::AccessPointData& data1,
                  const mojom::AccessPointData& data2) const {
    return data1.mac_address < data2.mac_address;
  }
};

// All data for wifi.
struct WifiData {
  WifiData();
  WifiData(const WifiData& other);
  WifiData& operator=(const WifiData& other);
  ~WifiData();

  // Determines whether a new set of WiFi data differs significantly from this.
  bool DiffersSignificantly(const WifiData& other) const;

  // Store access points as a set, sorted by MAC address. This allows quick
  // comparison of sets for detecting changes and for caching.
  typedef std::set<mojom::AccessPointData, AccessPointDataLess>
      AccessPointDataSet;
  AccessPointDataSet access_point_data;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_H_
