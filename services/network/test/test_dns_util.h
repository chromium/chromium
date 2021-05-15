// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_DNS_UTIL_H_
#define SERVICES_NETWORK_TEST_TEST_DNS_UTIL_H_

#include <stdint.h>

#include "net/base/address_list.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace mojom {
class NetworkContext;
}

// Struct containing the results passed to a network::mojom::ResolveHostClient's
// OnComplete() method.
struct DnsLookupResult {
  DnsLookupResult();
  DnsLookupResult(const DnsLookupResult& dns_lookup_result);
  ~DnsLookupResult();

  int32_t error;
  net::ResolveErrorInfo resolve_error_info;
  absl::optional<net::AddressList> resolved_addresses;
};

// Test utility function to perform the indicated DNS resolution, and block
// until complete. Only handles address results.
DnsLookupResult BlockingDnsLookup(
    mojom::NetworkContext* network_context,
    const net::HostPortPair& host_port_pair,
    network::mojom::ResolveHostParametersPtr params,
    const net::NetworkIsolationKey& network_isolation_key);

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_DNS_UTIL_H_
