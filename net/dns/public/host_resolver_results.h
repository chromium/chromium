// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_HOST_RESOLVER_RESULTS_H_
#define NET_DNS_PUBLIC_HOST_RESOLVER_RESULTS_H_

#include <optional>
#include <string>
#include <tuple>
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

  bool operator==(const HostResolverEndpointResult& other) const {
    return std::tie(ip_endpoints, metadata) ==
           std::tie(other.ip_endpoints, other.metadata);
  }
  bool operator!=(const HostResolverEndpointResult& other) const {
    return !(*this == other);
  }

  // IP endpoints at which to connect to the service.
  std::vector<net::IPEndPoint> ip_endpoints;

  // Additional metadata for creating connections to the endpoint. Typically
  // sourced from DNS HTTPS records.
  ConnectionEndpointMetadata metadata;
};

using HostResolverEndpointResults =
    std::vector<net::HostResolverEndpointResult>;

// Represents a result of a service endpoint resolution. Almost the identical
// to HostResolverEndpointResult, but has separate IPEndPoints for each address
// family.
struct NET_EXPORT_PRIVATE ServiceEndpoint {
  ServiceEndpoint();
  ~ServiceEndpoint();

  ServiceEndpoint(std::vector<IPEndPoint> ipv4_endpoints,
                  std::vector<IPEndPoint> ipv6_endpoints,
                  ConnectionEndpointMetadata metadata);

  ServiceEndpoint(const ServiceEndpoint&);
  ServiceEndpoint& operator=(const ServiceEndpoint&) = default;
  ServiceEndpoint(ServiceEndpoint&&);
  ServiceEndpoint& operator=(ServiceEndpoint&&) = default;

  bool operator==(const ServiceEndpoint& other) const {
    return std::forward_as_tuple(ipv4_endpoints, ipv6_endpoints, metadata) ==
           std::forward_as_tuple(other.ipv4_endpoints, other.ipv6_endpoints,
                                 other.metadata);
  }
  bool operator!=(const ServiceEndpoint& other) const {
    return !(*this == other);
  }

  base::Value::Dict ToValue() const;

  // IPv4 endpoints at which to connect to the service.
  std::vector<IPEndPoint> ipv4_endpoints;

  // IPv6 endpoints at which to connect to the service.
  std::vector<IPEndPoint> ipv6_endpoints;

  // Additional metadata for creating connections to the endpoint. Typically
  // sourced from DNS HTTPS records.
  // TODO(crbug.com/41493696): Consider inlining EchConfigList and ALPNs rather
  // than just using ConnectionEndpointMetadata.
  ConnectionEndpointMetadata metadata;
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_HOST_RESOLVER_RESULTS_H_
