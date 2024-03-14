// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/host_resolver_results.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

namespace net {

HostResolverEndpointResult::HostResolverEndpointResult() = default;
HostResolverEndpointResult::~HostResolverEndpointResult() = default;
HostResolverEndpointResult::HostResolverEndpointResult(
    const HostResolverEndpointResult&) = default;
HostResolverEndpointResult::HostResolverEndpointResult(
    HostResolverEndpointResult&&) = default;

ServiceEndpoint::ServiceEndpoint() = default;
ServiceEndpoint::~ServiceEndpoint() = default;

ServiceEndpoint::ServiceEndpoint(std::vector<IPEndPoint> ipv4_endpoints,
                                 std::vector<IPEndPoint> ipv6_endpoints,
                                 ConnectionEndpointMetadata metadata)
    : ipv4_endpoints(std::move(ipv4_endpoints)),
      ipv6_endpoints(std::move(ipv6_endpoints)),
      metadata(std::move(metadata)) {}

ServiceEndpoint::ServiceEndpoint(const ServiceEndpoint&) = default;
ServiceEndpoint::ServiceEndpoint(ServiceEndpoint&&) = default;

base::Value::Dict ServiceEndpoint::ToValue() const {
  base::Value::Dict dict;
  base::Value::List ipv4_endpoints_list;
  base::Value::List ipv6_endpoints_list;
  for (const auto& ip_endpoint : ipv4_endpoints) {
    ipv4_endpoints_list.Append(ip_endpoint.ToValue());
  }
  for (const auto& ip_endpoint : ipv6_endpoints) {
    ipv6_endpoints_list.Append(ip_endpoint.ToValue());
  }

  dict.Set("ipv4_endpoints", std::move(ipv4_endpoints_list));
  dict.Set("ipv6_endpoints", std::move(ipv6_endpoints_list));
  dict.Set("metadata", metadata.ToValue());
  return dict;
}

}  // namespace net
