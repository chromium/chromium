// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DNS_PROBER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DNS_PROBER_H_

#include "base/functional/callback.h"
#include "net/base/address_list.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace content {

// PrefetchDNSProber is a simple ResolveHostClient implementation that
// performs DNS resolution and invokes a callback upon completion.
class PrefetchDNSProber : public network::mojom::ResolveHostClient {
 public:
  using OnDNSResultsCallback =
      base::OnceCallback<void(int, const net::AddressList& resolved_addresses)>;

  explicit PrefetchDNSProber(OnDNSResultsCallback callback);
  ~PrefetchDNSProber() override;

  // network::mojom::ResolveHostClient:
  void OnTextResults(const std::vector<std::string>&) override {}
  void OnHostnameResults(const std::vector<net::HostPortPair>&) override {}
  void OnComplete(
      int32_t error,
      const net::ResolveErrorInfo& resolve_error_info,
      const net::AddressList& resolved_addresses,
      const net::HostResolverEndpointResults& alternative_endpoints) override;

 private:
  OnDNSResultsCallback callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DNS_PROBER_H_
