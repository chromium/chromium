// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces.h"

#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>

#include "net/base/winsock_init.h"
#endif

namespace net {

NetworkInterface::NetworkInterface()
    : type(NetworkChangeNotifier::CONNECTION_UNKNOWN), prefix_length(0) {
}

NetworkInterface::NetworkInterface(const std::string& name,
                                   const std::string& friendly_name,
                                   uint32_t interface_index,
                                   NetworkChangeNotifier::ConnectionType type,
                                   const IPAddress& address,
                                   uint32_t prefix_length,
                                   int ip_address_attributes,
                                   std::optional<Eui48MacAddress> mac_address)
    : name(name),
      friendly_name(friendly_name),
      interface_index(interface_index),
      type(type),
      address(address),
      prefix_length(prefix_length),
      ip_address_attributes(ip_address_attributes),
      mac_address(mac_address) {}

NetworkInterface::NetworkInterface(const NetworkInterface& other) = default;

NetworkInterface::~NetworkInterface() = default;

ScopedWifiOptions::~ScopedWifiOptions() = default;

std::string GetHostName() {
#if BUILDFLAG(IS_WIN)
  EnsureWinsockInit();
#endif

  // Host names are limited to 255 bytes.
  char buffer[256];
  int result = gethostname(buffer, sizeof(buffer));
  if (result != 0) {
    DVLOG(1) << "gethostname() failed with " << result;
    buffer[0] = '\0';
  }
  return std::string(buffer);
}

}  // namespace net
