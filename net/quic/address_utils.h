// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_ADDRESS_UTILS_H_
#define NET_QUIC_ADDRESS_UTILS_H_

#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ip_address.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ip_address_family.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_socket_address.h"

namespace net {

inline IPEndPoint ToIPEndPoint(quic::QuicSocketAddress address) {
  if (!address.IsInitialized()) {
    return IPEndPoint();
  }

  IPEndPoint result;
  sockaddr_storage storage = address.generic_address();
  const bool success = result.FromSockAddr(
      reinterpret_cast<const sockaddr*>(&storage), sizeof(storage));
  DCHECK(success);
  return result;
}

inline IPAddress ToIPAddress(quic::QuicIpAddress address) {
  if (!address.IsInitialized()) {
    return IPAddress();
  }

  switch (address.address_family()) {
    case quic::IpAddressFamily::IP_V4: {
      in_addr raw_address = address.GetIPv4();
      return IPAddress(reinterpret_cast<const uint8_t*>(&raw_address),
                       sizeof(raw_address));
    }
    case quic::IpAddressFamily::IP_V6: {
      in6_addr raw_address = address.GetIPv6();
      return IPAddress(reinterpret_cast<const uint8_t*>(&raw_address),
                       sizeof(raw_address));
    }
    default:
      DCHECK_EQ(address.address_family(), quic::IpAddressFamily::IP_UNSPEC);
      return IPAddress();
  }
}

inline quic::QuicSocketAddress ToQuicSocketAddress(IPEndPoint address) {
  if (address.address().empty()) {
    return quic::QuicSocketAddress();
  }

  sockaddr_storage result;
  socklen_t size = sizeof(result);
  bool success =
      address.ToSockAddr(reinterpret_cast<sockaddr*>(&result), &size);
  DCHECK(success);
  return quic::QuicSocketAddress(result);
}

inline quic::QuicIpAddress ToQuicIpAddress(net::IPAddress address) {
  if (address.IsIPv4()) {
    in_addr result;
    static_assert(sizeof(result) == IPAddress::kIPv4AddressSize,
                  "Address size mismatch");
    memcpy(&result, address.bytes().data(), IPAddress::kIPv4AddressSize);
    return quic::QuicIpAddress(result);
  }
  if (address.IsIPv6()) {
    in6_addr result;
    static_assert(sizeof(result) == IPAddress::kIPv6AddressSize,
                  "Address size mismatch");
    memcpy(&result, address.bytes().data(), IPAddress::kIPv6AddressSize);
    return quic::QuicIpAddress(result);
  }

  DCHECK(address.empty());
  return quic::QuicIpAddress();
}

}  // namespace net

#endif  // NET_QUIC_ADDRESS_UTILS_H_
