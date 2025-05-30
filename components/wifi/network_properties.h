// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_NETWORK_PROPERTIES_H_
#define COMPONENTS_WIFI_NETWORK_PROPERTIES_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <string>

#include "base/values.h"
#include "components/onc/onc_constants.h"
#include "components/wifi/wifi_export.h"

namespace wifi {

typedef int32_t Frequency;

enum FrequencyEnum {
  kFrequencyAny = 0,
  kFrequencyUnknown = 0,
  kFrequency2400 = 2400,
  kFrequency5000 = 5000
};

typedef std::set<Frequency> FrequencySet;

// Network Properties, can be used to parse the result of |GetProperties| and
// |GetVisibleNetworks|.
struct WIFI_EXPORT NetworkProperties {
  NetworkProperties();
  NetworkProperties(const NetworkProperties& other);
  NetworkProperties& operator=(const NetworkProperties& other);
  NetworkProperties(NetworkProperties&& other);
  NetworkProperties& operator=(NetworkProperties&& other);
  ~NetworkProperties();

  std::string connection_state = onc::connection_state::kNotConnected;
  std::string guid;
  std::string name;
  std::string ssid;
  std::string bssid;
  std::string type;
  std::string security = onc::wifi::kSecurityNone;
  // |password| field is used to pass wifi password for network creation via
  // |CreateNetwork| or connection via |StartConnect|. It does not persist
  // once operation is completed.
  std::string password;
  // WiFi Signal Strength. 0..100
  uint32_t signal_strength = 0;
  bool auto_connect = false;
  Frequency frequency = kFrequencyUnknown;
  FrequencySet frequency_set;

  base::Value::Dict ToValue(bool network_list) const;
  // Updates only properties set in |value|.
  bool UpdateFromValue(const base::Value::Dict& value);
  static std::string MacAddressAsString(const uint8_t mac_as_int[6]);
  static bool OrderByType(const NetworkProperties& l,
                          const NetworkProperties& r);

  friend bool operator==(const NetworkProperties& lhs,
                         const NetworkProperties& rhs) = default;
};

typedef std::list<NetworkProperties> NetworkList;

}  // namespace wifi

#endif  // COMPONENTS_WIFI_NETWORK_PROPERTIES_H_
