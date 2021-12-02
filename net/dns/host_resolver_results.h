// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_RESULTS_H_
#define NET_DNS_HOST_RESOLVER_RESULTS_H_

#include <string>
#include <vector>

#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"

namespace net {

// Host-resolution-result representation of a single endpoint and the
// information necessary to attempt a connection to that endpoint.
struct NET_EXPORT_PRIVATE HostResolverEndpointResult {
  HostResolverEndpointResult();
  ~HostResolverEndpointResult();

  HostResolverEndpointResult(const HostResolverEndpointResult&);
  HostResolverEndpointResult& operator=(const HostResolverEndpointResult&) =
      default;
  HostResolverEndpointResult(HostResolverEndpointResult&&);
  HostResolverEndpointResult& operator=(HostResolverEndpointResult&&) = default;

  // IP endpoints at which to connect to the service.
  std::vector<net::IPEndPoint> ip_endpoints;

  // The final name in the alias chain (DNS CNAME or HTTPS) at which the
  // IPv4 addresses were found.
  std::string ipv4_alias_name;

  // The final name in the alias chain (DNS CNAME or HTTPS) at which the
  // IPv6 addresses were found.
  std::string ipv6_alias_name;

  // Additional metadata for creating connections to the endpoint. Typically
  // sourced from DNS HTTPS records.
  ConnectionEndpointMetadata metadata;
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_RESULTS_H_
