// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/dns_request.h"

#include "base/strings/string_split.h"
#include "net/base/net_errors.h"
#include "net/dns/public/dns_query_type.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/scheme_host_port.h"

namespace content::webid {

DnsRequest::DnsRequest(network::NetworkContextGetter network_context_getter)
    : network_context_getter_(std::move(network_context_getter)) {}

DnsRequest::~DnsRequest() = default;

void DnsRequest::SendRequest(const std::string& hostname,
                             DnsRequestCallback callback) {
  callback_ = std::move(callback);

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::TXT;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;
  auto* network_context = network_context_getter_.Run();
  if (!network_context) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  network_context->CreateHostResolver(
      std::nullopt, host_resolver_.BindNewPipeAndPassReceiver());

  host_resolver_->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                                  net::HostPortPair(hostname, 0)),
                              net::NetworkAnonymizationKey(),
                              std::move(parameters),
                              receiver_.BindNewPipeAndPassRemote());
}

void DnsRequest::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const net::AddressList& resolved_addresses,
    const std::vector<net::HostResolverEndpointResult>& alternative_endpoints) {
  if (result != net::OK) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  // If there are text results, OnTextResults is called first, so callback_
  // will be null because it has already been std::moved and run.
  // If callback_ is still valid, it means OnTextResults was not called,
  // so we return an empty vector to indicate that there were no TXT
  // records.
  if (callback_) {
    std::move(callback_).Run(std::vector<std::string>());
  }
}

void DnsRequest::OnTextResults(const std::vector<std::string>& text_results) {
  std::move(callback_).Run(text_results);
}

void DnsRequest::OnHostnameResults(
    const std::vector<net::HostPortPair>& hosts) {}

}  // namespace content::webid
