// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_dns_util.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

DnsLookupResult::DnsLookupResult(
    int32_t error,
    net::ResolveErrorInfo resolve_error_info,
    std::optional<net::AddressList> resolved_addresses,
    std::optional<net::HostResolverEndpointResults>
        endpoint_results_with_metadata)
    : error(error),
      resolve_error_info(std::move(resolve_error_info)),
      resolved_addresses(std::move(resolved_addresses)),
      endpoint_results_with_metadata(
          std::move(endpoint_results_with_metadata)) {}

DnsLookupResult::DnsLookupResult(const DnsLookupResult& dns_lookup_result) =
    default;
DnsLookupResult::~DnsLookupResult() = default;

DnsLookupResult BlockingDnsLookup(
    mojom::NetworkContext* network_context,
    const net::HostPortPair& host_port_pair,
    network::mojom::ResolveHostParametersPtr params,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  base::test::TestFuture<int32_t, const net::ResolveErrorInfo&,
                         const std::optional<net::AddressList>&,
                         const std::optional<net::HostResolverEndpointResults>&>
      future;
  auto resolver = SimpleHostResolver::Create(network_context);
  // TODO(crbug.com/40235854): Consider passing a SchemeHostPort to trigger
  // HTTPS DNS resource record query.
  resolver->ResolveHost(
      mojom::HostResolverHost::NewHostPortPair(host_port_pair),
      network_anonymization_key, std::move(params), future.GetCallback());
  auto [error, resolve_error_info, resolved_addresses,
        endpoint_results_with_metadata] = future.Take();
  return DnsLookupResult(error, std::move(resolve_error_info),
                         std::move(resolved_addresses),
                         std::move(endpoint_results_with_metadata));
}

}  // namespace network
