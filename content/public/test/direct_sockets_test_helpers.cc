// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/direct_sockets_test_helpers.h"

#include <stdint.h>

#include "base/threading/thread_restrictions.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"

namespace content {

std::vector<std::string> DeriveSsmSourceAddresses(size_t count) {
  net::NetworkInterfaceList interfaces;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!net::GetNetworkList(&interfaces,
                             net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
      return {};
    }
  }

  const net::IPAddress* local_ip = nullptr;
  for (const auto& network_interface : interfaces) {
    if (network_interface.address.IsIPv4() &&
        !network_interface.address.IsLoopback()) {
      local_ip = &network_interface.address;
      break;
    }
  }
  if (!local_ip) {
    return {};
  }

  auto bytes = local_ip->bytes();
  const uint8_t local_last = bytes.back();
  std::vector<std::string> result;
  for (uint8_t candidate = 1; result.size() < count; ++candidate) {
    if (candidate != local_last) {
      bytes.back() = candidate;
      result.push_back(net::IPAddress(bytes).ToString());
    }
  }
  return result;
}

}  // namespace content
