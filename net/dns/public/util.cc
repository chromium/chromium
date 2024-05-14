// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/util.h"

#include <stdint.h>

#include <string_view>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "net/base/ip_address.h"
#include "net/dns/public/dns_protocol.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

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
      NOTREACHED_IN_MIGRATION();
      return IPEndPoint();
  }
}

IPEndPoint GetMdnsReceiveEndPoint(AddressFamily address_family) {
// TODO(qingsi): MacOS should follow other POSIX platforms in the else-branch
// after addressing crbug.com/899310. We have encountered a conflicting issue on
// CrOS as described in crbug.com/931916, and the following is a temporary
// mitigation to reconcile the two issues. Remove this after closing
// crbug.com/899310.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
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
      NOTREACHED_IN_MIGRATION();
      return IPEndPoint();
  }
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // With POSIX/Fuchsia, any socket can receive messages for multicast groups
  // joined by any socket on the system. Sockets intending to receive messages
  // for a specific multicast group should bind to that group address.
  return GetMdnsGroupEndPoint(address_family);
#else
#error Platform not supported.
#endif
}

std::string GetNameForHttpsQuery(const url::SchemeHostPort& scheme_host_port,
                                 uint16_t* out_port) {
  DCHECK(!scheme_host_port.host().empty() &&
         scheme_host_port.host().front() != '.');

  // Normalize ws/wss schemes to http/https. Note that this behavior is not
  // indicated by the draft-ietf-dnsop-svcb-https-08 spec.
  std::string_view normalized_scheme = scheme_host_port.scheme();
  if (normalized_scheme == url::kWsScheme) {
    normalized_scheme = url::kHttpScheme;
  } else if (normalized_scheme == url::kWssScheme) {
    normalized_scheme = url::kHttpsScheme;
  }

  // For http-schemed hosts, request the corresponding upgraded https host
  // per the rules in draft-ietf-dnsop-svcb-https-08, Section 9.5.
  uint16_t port = scheme_host_port.port();
  if (normalized_scheme == url::kHttpScheme) {
    normalized_scheme = url::kHttpsScheme;
    if (port == 80)
      port = 443;
  }

  // Scheme should always end up normalized to "https" to create HTTPS
  // transactions.
  DCHECK_EQ(normalized_scheme, url::kHttpsScheme);

  if (out_port != nullptr)
    *out_port = port;

  // Per the rules in draft-ietf-dnsop-svcb-https-08, Section 9.1 and 2.3,
  // encode scheme and port in the transaction hostname, unless the port is
  // the default 443.
  if (port == 443)
    return scheme_host_port.host();
  return base::StrCat({"_", base::NumberToString(scheme_host_port.port()),
                       "._https.", scheme_host_port.host()});
}

}  // namespace dns_util
}  // namespace net
