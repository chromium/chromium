// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/util.h"

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "net/base/ip_address.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

namespace {

IPEndPoint GetMdnsIPEndPoint(const char* address) {
  IPAddress multicast_group_number;
  bool success = multicast_group_number.AssignFromIPLiteral(address);
  DCHECK(success);
  return IPEndPoint(multicast_group_number,
                    dns_protocol::kDefaultPortMulticast);
}

}  // namespace

namespace dns_util {

IPEndPoint GetMdnsGroupEndPoint(AddressFamily address_family) {
  switch (address_family) {
    case ADDRESS_FAMILY_IPV4:
      return GetMdnsIPEndPoint(dns_protocol::kMdnsMulticastGroupIPv4);
    case ADDRESS_FAMILY_IPV6:
      return GetMdnsIPEndPoint(dns_protocol::kMdnsMulticastGroupIPv6);
    default:
      NOTREACHED();
      return IPEndPoint();
  }
}

IPEndPoint GetMdnsReceiveEndPoint(AddressFamily address_family) {
// TODO(qingsi): MacOS should follow other POSIX platforms in the else-branch
// after addressing crbug.com/899310. We have encountered a conflicting issue on
// CrOS as described in crbug.com/931916, and the following is a temporary
// mitigation to reconcile the two issues. Remove this after closing
// crbug.com/899310.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_APPLE)
  // With Windows, binding to a mulitcast group address is not allowed.
  // Multicast messages will be received appropriate to the multicast groups the
  // socket has joined. Sockets intending to receive multicast messages should
  // bind to a wildcard address (e.g. 0.0.0.0).
  switch (address_family) {
    case ADDRESS_FAMILY_IPV4:
      return IPEndPoint(IPAddress::IPv4AllZeros(),
                        dns_protocol::kDefaultPortMulticast);
    case ADDRESS_FAMILY_IPV6:
      return IPEndPoint(IPAddress::IPv6AllZeros(),
                        dns_protocol::kDefaultPortMulticast);
    default:
      NOTREACHED();
      return IPEndPoint();
  }
#else   // !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)) || BUILDFLAG(IS_APPLE)
  // With POSIX, any socket can receive messages for multicast groups joined by
  // any socket on the system. Sockets intending to receive messages for a
  // specific multicast group should bind to that group address.
  return GetMdnsGroupEndPoint(address_family);
#endif  // !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)) || BUILDFLAG(IS_APPLE)
}

}  // namespace dns_util
}  // namespace net
