// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/wifi/network_properties.h"

#include <optional>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/onc/onc_constants.h"

namespace wifi {

NetworkProperties::NetworkProperties()
    : connection_state(onc::connection_state::kNotConnected),
      security(onc::wifi::kSecurityNone),
      signal_strength(0),
      auto_connect(false),
      frequency(kFrequencyUnknown) {
}

NetworkProperties::NetworkProperties(const NetworkProperties& other) = default;

NetworkProperties::~NetworkProperties() {
}

base::Value::Dict NetworkProperties::ToValue(bool network_list) const {
  base::Value::Dict value;

  value.Set(onc::network_config::kGUID, guid);
  value.Set(onc::network_config::kName, name);
  value.Set(onc::network_config::kConnectionState, connection_state);
  DCHECK(type == onc::network_type::kWiFi);
  value.Set(onc::network_config::kType, type);

  // For now, assume all WiFi services are connectable.
  value.Set(onc::network_config::kConnectable, true);

  base::Value::Dict wifi;
  wifi.Set(onc::wifi::kSecurity, security);
  wifi.Set(onc::wifi::kSignalStrength, static_cast<int>(signal_strength));

  // Network list expects subset of data.
  if (!network_list) {
    if (frequency != kFrequencyUnknown)
      wifi.Set(onc::wifi::kFrequency, frequency);
    base::Value::List frequency_list;
    for (FrequencySet::const_iterator it = this->frequency_set.begin();
         it != this->frequency_set.end();
         ++it) {
      frequency_list.Append(*it);
    }
    if (!frequency_list.empty()) {
      wifi.Set(onc::wifi::kFrequencyList, std::move(frequency_list));
    }
    if (!bssid.empty())
      wifi.Set(onc::wifi::kBSSID, bssid);
    wifi.Set(onc::wifi::kSSID, ssid);
    wifi.Set(onc::wifi::kHexSSID, base::HexEncode(ssid));
  }
  value.Set(onc::network_type::kWiFi, std::move(wifi));

  return value;
}

bool NetworkProperties::UpdateFromValue(const base::Value::Dict& value) {
  const std::string* network_type =
      value.FindString(onc::network_config::kType);
  // Get network type and make sure that it is WiFi (if specified).
  if (network_type) {
    if (*network_type != onc::network_type::kWiFi)
      return false;
    type = *network_type;
  }

  const base::Value::Dict* wifi = value.FindDict(onc::network_type::kWiFi);
  if (wifi) {
    const std::string* name_ptr = value.FindString(onc::network_config::kName);
    if (name_ptr)
      name = *name_ptr;
    const std::string* guid_ptr = value.FindString(onc::network_config::kGUID);
    if (guid_ptr)
      guid = *guid_ptr;
    const std::string* connection_state_ptr =
        value.FindString(onc::network_config::kConnectionState);
    if (connection_state_ptr)
      connection_state = *connection_state_ptr;

    const std::string* security_ptr = wifi->FindString(onc::wifi::kSecurity);
    if (security_ptr)
      security = *security_ptr;
    const std::string* ssid_ptr = wifi->FindString(onc::wifi::kSSID);
    if (ssid_ptr)
      ssid = *ssid_ptr;
    const std::string* password_ptr = wifi->FindString(onc::wifi::kPassphrase);
    if (password_ptr)
      password = *password_ptr;

    std::optional<bool> auto_connect_opt =
        wifi->FindBool(onc::wifi::kAutoConnect);
    if (auto_connect_opt)
      auto_connect = *auto_connect_opt;

    return true;
  }
  return false;
}

std::string NetworkProperties::MacAddressAsString(const uint8_t mac_as_int[6]) {
  // mac_as_int is big-endian. Write in byte chunks.
  // Format is XX:XX:XX:XX:XX:XX.
  static const char* const kMacFormatString = "%02x:%02x:%02x:%02x:%02x:%02x";
  return base::StringPrintf(kMacFormatString,
                            mac_as_int[0],
                            mac_as_int[1],
                            mac_as_int[2],
                            mac_as_int[3],
                            mac_as_int[4],
                            mac_as_int[5]);
}

bool NetworkProperties::OrderByType(const NetworkProperties& l,
                                    const NetworkProperties& r) {
  if (l.connection_state != r.connection_state)
    return l.connection_state < r.connection_state;
  // This sorting order is needed only for browser_tests, which expect this
  // network type sort order: ethernet < wifi < vpn < cellular.
  if (l.type == r.type)
    return l.guid < r.guid;
  if (l.type == onc::network_type::kEthernet)
    return true;
  if (r.type == onc::network_type::kEthernet)
    return false;
  return l.type > r.type;
}

}  // namespace wifi
