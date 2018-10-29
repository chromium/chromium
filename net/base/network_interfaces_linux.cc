// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces_linux.h"

#include <memory>

#if !defined(OS_ANDROID)
#include <linux/ethtool.h>
#endif  // !defined(OS_ANDROID)
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/wireless.h>
#include <set>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/address_tracker_linux.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces_posix.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace net {

namespace {

// When returning true, the platform native IPv6 address attributes were
// successfully converted to net IP address attributes. Otherwise, returning
// false and the caller should drop the IP address which can't be used by the
// application layer.
bool TryConvertNativeToNetIPAttributes(int native_attributes,
                                       int* net_attributes) {
  // For Linux/ChromeOS/Android, we disallow addresses with attributes
  // IFA_F_OPTIMISTIC, IFA_F_DADFAILED, and IFA_F_TENTATIVE as these
  // are still progressing through duplicated address detection (DAD)
  // and shouldn't be used by the application layer until DAD process
  // is completed.
  if (native_attributes & (
#if !defined(OS_ANDROID)
                              IFA_F_OPTIMISTIC | IFA_F_DADFAILED |
#endif  // !OS_ANDROID
                              IFA_F_TENTATIVE)) {
    return false;
  }

  if (native_attributes & IFA_F_TEMPORARY) {
    *net_attributes |= IP_ADDRESS_ATTRIBUTE_TEMPORARY;
  }

  if (native_attributes & IFA_F_DEPRECATED) {
    *net_attributes |= IP_ADDRESS_ATTRIBUTE_DEPRECATED;
  }

  return true;
}

}  // namespace

namespace internal {

// Gets the connection type for interface |ifname| by checking for wireless
// or ethtool extensions.
NetworkChangeNotifier::ConnectionType GetInterfaceConnectionType(
    const std::string& ifname) {
  base::ScopedFD s = GetSocketForIoctl();
  if (!s.is_valid())
    return NetworkChangeNotifier::CONNECTION_UNKNOWN;

  // Test wireless extensions for CONNECTION_WIFI
  struct iwreq pwrq = {};
  strncpy(pwrq.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
  if (ioctl(s.get(), SIOCGIWNAME, &pwrq) != -1)
    return NetworkChangeNotifier::CONNECTION_WIFI;

#if !defined(OS_ANDROID)
  // Test ethtool for CONNECTION_ETHERNET
  struct ethtool_cmd ecmd = {};
  ecmd.cmd = ETHTOOL_GSET;
  struct ifreq ifr = {};
  ifr.ifr_data = &ecmd;
  strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
  if (ioctl(s.get(), SIOCETHTOOL, &ifr) != -1)
    return NetworkChangeNotifier::CONNECTION_ETHERNET;
#endif  // !defined(OS_ANDROID)

  return NetworkChangeNotifier::CONNECTION_UNKNOWN;
}

std::string GetInterfaceSSID(const std::string& ifname) {
  base::ScopedFD ioctl_socket = GetSocketForIoctl();
  if (!ioctl_socket.is_valid())
    return std::string();
  struct iwreq wreq = {};
  strncpy(wreq.ifr_name, ifname.c_str(), IFNAMSIZ - 1);

  char ssid[IW_ESSID_MAX_SIZE + 1] = {0};
  wreq.u.essid.pointer = ssid;
  wreq.u.essid.length = IW_ESSID_MAX_SIZE;
  if (ioctl(ioctl_socket.get(), SIOCGIWESSID, &wreq) != -1)
    return ssid;
  return std::string();
}

bool GetNetworkListImpl(
    NetworkInterfaceList* networks,
    int policy,
    const std::unordered_set<int>& online_links,
    const internal::AddressTrackerLinux::AddressMap& address_map,
    GetInterfaceNameFunction get_interface_name) {
  std::map<int, std::string> ifnames;

  for (auto it = address_map.begin(); it != address_map.end(); ++it) {
    // Ignore addresses whose links are not online.
    if (online_links.find(it->second.ifa_index) == online_links.end())
      continue;

    sockaddr_storage sock_addr;
    socklen_t sock_len = sizeof(sockaddr_storage);

    // Convert to sockaddr for next check.
    if (!IPEndPoint(it->first, 0)
             .ToSockAddr(reinterpret_cast<sockaddr*>(&sock_addr), &sock_len)) {
      continue;
    }

    // Skip unspecified addresses (i.e. made of zeroes) and loopback addresses
    if (IsLoopbackOrUnspecifiedAddress(reinterpret_cast<sockaddr*>(&sock_addr)))
      continue;

    int ip_attributes = IP_ADDRESS_ATTRIBUTE_NONE;

    if (it->second.ifa_family == AF_INET6) {
      // Ignore addresses whose attributes are not actionable by
      // the application layer.
      if (!TryConvertNativeToNetIPAttributes(it->second.ifa_flags,
                                             &ip_attributes))
        continue;
    }

    // Find the name of this link.
    std::map<int, std::string>::const_iterator itname =
        ifnames.find(it->second.ifa_index);
    std::string ifname;
    if (itname == ifnames.end()) {
      char buffer[IFNAMSIZ] = {0};
      ifname.assign(get_interface_name(it->second.ifa_index, buffer));
      // Ignore addresses whose interface name can't be retrieved.
      if (ifname.empty())
        continue;
      ifnames[it->second.ifa_index] = ifname;
    } else {
      ifname = itname->second;
    }

    // Based on the interface name and policy, determine whether we
    // should ignore it.
    if (ShouldIgnoreInterface(ifname, policy))
      continue;

    NetworkChangeNotifier::ConnectionType type =
        GetInterfaceConnectionType(ifname);

    networks->push_back(
        NetworkInterface(ifname, ifname, it->second.ifa_index, type, it->first,
                         it->second.ifa_prefixlen, ip_attributes));
  }

  return true;
}

std::string GetWifiSSIDFromInterfaceListInternal(
    const NetworkInterfaceList& interfaces,
    internal::GetInterfaceSSIDFunction get_interface_ssid) {
  std::string connected_ssid;
  for (size_t i = 0; i < interfaces.size(); ++i) {
    if (interfaces[i].type != NetworkChangeNotifier::CONNECTION_WIFI)
      return std::string();
    std::string ssid = get_interface_ssid(interfaces[i].name);
    if (i == 0) {
      connected_ssid = ssid;
    } else if (ssid != connected_ssid) {
      return std::string();
    }
  }
  return connected_ssid;
}

base::ScopedFD GetSocketForIoctl() {
  base::ScopedFD ioctl_socket(socket(AF_INET6, SOCK_DGRAM, 0));
  if (ioctl_socket.is_valid())
    return ioctl_socket;
  return base::ScopedFD(socket(AF_INET, SOCK_DGRAM, 0));
}

}  // namespace internal

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  if (networks == NULL)
    return false;

  internal::AddressTrackerLinux tracker;
  tracker.Init();

  return internal::GetNetworkListImpl(
      networks, policy, tracker.GetOnlineLinks(), tracker.GetAddressMap(),
      &internal::AddressTrackerLinux::GetInterfaceName);
}

std::string GetWifiSSID() {
// On Android, obtain the SSID using the Android-specific APIs.
#if defined(OS_ANDROID)
  return android::GetWifiSSID();
#endif
  NetworkInterfaceList networks;
  if (GetNetworkList(&networks, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    return internal::GetWifiSSIDFromInterfaceListInternal(
        networks, internal::GetInterfaceSSID);
  }
  return std::string();
}

}  // namespace net
