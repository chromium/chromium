// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_INTERFACES_H_
#define NET_BASE_NETWORK_INTERFACES_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {

// A subset of IP address attributes which are actionable by the
// application layer. Currently unimplemented for all hosts;
// IP_ADDRESS_ATTRIBUTE_NONE is always returned.
enum IPAddressAttributes {
  IP_ADDRESS_ATTRIBUTE_NONE = 0,

  // A temporary address is dynamic by nature and will not contain MAC
  // address. Presence of MAC address in IPv6 addresses can be used to
  // track an endpoint and cause privacy concern. Please refer to
  // RFC4941.
  IP_ADDRESS_ATTRIBUTE_TEMPORARY = 1 << 0,

  // A temporary address could become deprecated once the preferred
  // lifetime is reached. It is still valid but shouldn't be used to
  // create new connections.
  IP_ADDRESS_ATTRIBUTE_DEPRECATED = 1 << 1,

  // Anycast address.
  IP_ADDRESS_ATTRIBUTE_ANYCAST = 1 << 2,

  // Tentative address.
  IP_ADDRESS_ATTRIBUTE_TENTATIVE = 1 << 3,

  // DAD detected duplicate.
  IP_ADDRESS_ATTRIBUTE_DUPLICATED = 1 << 4,

  // May be detached from the link.
  IP_ADDRESS_ATTRIBUTE_DETACHED = 1 << 5,
};

using Eui48MacAddress = std::array<uint8_t, 6>;

// struct that is used by GetNetworkList() to represent a network
// interface.
struct NET_EXPORT NetworkInterface {
  NetworkInterface();
  NetworkInterface(const std::string& name,
                   const std::string& friendly_name,
                   uint32_t interface_index,
                   NetworkChangeNotifier::ConnectionType type,
                   const IPAddress& address,
                   uint32_t prefix_length,
                   int ip_address_attributes,
                   std::optional<Eui48MacAddress> mac_address = std::nullopt);
  NetworkInterface(const NetworkInterface& other);
  ~NetworkInterface();

  std::string name;
  std::string friendly_name;  // Same as |name| on non-Windows.
  uint32_t interface_index;  // Always 0 on Android.
  NetworkChangeNotifier::ConnectionType type;
  IPAddress address;
  uint32_t prefix_length;
  int ip_address_attributes;  // Combination of |IPAddressAttributes|.
  std::optional<Eui48MacAddress> mac_address;
};

typedef std::vector<NetworkInterface> NetworkInterfaceList;

// Policy settings to include/exclude network interfaces.
enum HostAddressSelectionPolicy {
  INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES           = 0x0,
  EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES           = 0x1,
};

// Returns list of network interfaces except loopback interface. If an
// interface has more than one address, a separate entry is added to
// the list for each address.
// Can be called only on a thread that allows IO.
NET_EXPORT bool GetNetworkList(NetworkInterfaceList* networks,
                               int policy);

// Gets the SSID of the currently associated WiFi access point if there is one,
// and it is available. SSID may not be available if the app does not have
// permissions to access it. On Android M+, the app accessing SSID needs to have
// ACCESS_COARSE_LOCATION or ACCESS_FINE_LOCATION. If there is no WiFi access
// point or its SSID is unavailable, an empty string is returned.
// Currently only implemented on Linux, ChromeOS, Android and Windows.
NET_EXPORT std::string GetWifiSSID();

// General category of the IEEE 802.11 (wifi) physical layer operating mode.
enum WifiPHYLayerProtocol {
  // No wifi support or no associated AP.
  WIFI_PHY_LAYER_PROTOCOL_NONE,
  // An obsolete modes introduced by the original 802.11, e.g. IR, FHSS.
  WIFI_PHY_LAYER_PROTOCOL_ANCIENT,
  // 802.11a, OFDM-based rates.
  WIFI_PHY_LAYER_PROTOCOL_A,
  // 802.11b, DSSS or HR DSSS.
  WIFI_PHY_LAYER_PROTOCOL_B,
  // 802.11g, same rates as 802.11a but compatible with 802.11b.
  WIFI_PHY_LAYER_PROTOCOL_G,
  // 802.11n, HT rates.
  WIFI_PHY_LAYER_PROTOCOL_N,
  // Unclassified mode or failure to identify.
  WIFI_PHY_LAYER_PROTOCOL_UNKNOWN,
  // 802.11ac
  WIFI_PHY_LAYER_PROTOCOL_AC,
  // 802.11ad
  WIFI_PHY_LAYER_PROTOCOL_AD,
  // 802.11ax
  WIFI_PHY_LAYER_PROTOCOL_AX
};

// Characterize the PHY mode of the currently associated access point.
// Currently only available on Windows.
NET_EXPORT WifiPHYLayerProtocol GetWifiPHYLayerProtocol();

enum WifiOptions {
  // Disables background SSID scans.
  WIFI_OPTIONS_DISABLE_SCAN =  1 << 0,
  // Enables media streaming mode.
  WIFI_OPTIONS_MEDIA_STREAMING_MODE = 1 << 1
};

class NET_EXPORT ScopedWifiOptions {
 public:
  ScopedWifiOptions() = default;
  ScopedWifiOptions(const ScopedWifiOptions&) = delete;
  ScopedWifiOptions& operator=(const ScopedWifiOptions&) = delete;
  virtual ~ScopedWifiOptions();
};

// Set temporary options on all wifi interfaces.
// |options| is an ORed bitfield of WifiOptions.
// Options are automatically disabled when the scoped pointer
// is freed. Currently only available on Windows.
NET_EXPORT std::unique_ptr<ScopedWifiOptions> SetWifiOptions(int options);

// Returns the hostname of the current system. Returns empty string on failure.
NET_EXPORT std::string GetHostName();

}  // namespace net

#endif  // NET_BASE_NETWORK_INTERFACES_H_
