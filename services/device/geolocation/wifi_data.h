// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_H_

#include <set>
#include <string>


namespace device {

// Wifi data relating to a single access point.
struct AccessPointData {
  AccessPointData();
  ~AccessPointData();

  // MAC address, formatted as per MacAddressAsString16.
  std::u16string mac_address;
  int radio_signal_strength;  // Measured in dBm
  int channel;
  int signal_to_noise;  // Ratio in dB
};

// This is to allow AccessPointData to be used in std::set. We order
// lexicographically by MAC address.
struct AccessPointDataLess {
  bool operator()(const AccessPointData& data1,
                  const AccessPointData& data2) const {
    return data1.mac_address < data2.mac_address;
  }
};

// All data for wifi.
struct WifiData {
  WifiData();
  WifiData(const WifiData& other);
  ~WifiData();

  // Determines whether a new set of WiFi data differs significantly from this.
  bool DiffersSignificantly(const WifiData& other) const;

  // Store access points as a set, sorted by MAC address. This allows quick
  // comparison of sets for detecting changes and for caching.
  typedef std::set<AccessPointData, AccessPointDataLess> AccessPointDataSet;
  AccessPointDataSet access_point_data;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_H_
