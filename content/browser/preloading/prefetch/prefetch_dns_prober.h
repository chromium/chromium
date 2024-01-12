// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DNS_PROBER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DNS_PROBER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace content {

// PrefetchDNSProber is a simple ResolveHostClient implementation that
// performs DNS resolution and invokes a callback upon completion.
class PrefetchDNSProber : public network::mojom::ResolveHostClient {
 public:
  using OnDNSResultsCallback = base::OnceCallback<
      void(int, const std::optional<net::AddressList>& resolved_addresses)>;

  explicit PrefetchDNSProber(OnDNSResultsCallback callback);
  ~PrefetchDNSProber() override;

  // network::mojom::ResolveHostClient:
  void OnTextResults(const std::vector<std::string>&) override {}
  void OnHostnameResults(const std::vector<net::HostPortPair>&) override {}
  void OnComplete(int32_t error,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

 private:
  OnDNSResultsCallback callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DNS_PROBER_H_