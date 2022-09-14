// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_HOST_RESOLVER_RESULTS_H_
#define NET_DNS_PUBLIC_HOST_RESOLVER_RESULTS_H_

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

}  // namespace net

#endif  // NET_DNS_PUBLIC_HOST_RESOLVER_RESULTS_H_
