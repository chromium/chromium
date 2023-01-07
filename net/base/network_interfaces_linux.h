// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_INTERFACES_LINUX_H_
#define NET_BASE_NETWORK_INTERFACES_LINUX_H_

// This file is only used to expose some of the internals
// of network_interfaces_linux.cc to address_tracker_linux and tests.

#include <string>
#include <unordered_set>

#include "base/files/scoped_file.h"
#include "net/base/address_tracker_linux.h"
#include "net/base/net_export.h"
#include "net/base/network_interfaces.h"

namespace net::internal {

typedef char* (*GetInterfaceNameFunction)(int interface_index, char* ifname);

typedef std::string (*GetInterfaceSSIDFunction)(const std::string& ifname);

NET_EXPORT bool GetNetworkListImpl(
    NetworkInterfaceList* networks,
    int policy,
    const std::unordered_set<int>& online_links,
    const internal::AddressTrackerLinux::AddressMap& address_map,
    GetInterfaceNameFunction get_interface_name);

// Gets the current Wi-Fi SSID based on |interfaces|. Returns
// empty string if there are no interfaces or if two interfaces have different
// connection types. Otherwise returns the SSID of all interfaces if they have
// the same SSID. This is adapted from
// NetworkChangeNotifier::ConnectionTypeFromInterfaceList.
NET_EXPORT std::string GetWifiSSIDFromInterfaceListInternal(
    const NetworkInterfaceList& interfaces,
    internal::GetInterfaceSSIDFunction get_interface_ssid);

// Returns a socket useful for performing ioctl()s.
base::ScopedFD GetSocketForIoctl();

}  // namespace net::internal

#endif  // NET_BASE_NETWORK_INTERFACES_LINUX_H_
