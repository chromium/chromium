// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_network_context_with_host_resolver.h"

namespace network {

TestNetworkContextWithHostResolver::TestNetworkContextWithHostResolver(
    std::unique_ptr<net::HostResolver> host_resolver)
    : host_resolver_(std::move(host_resolver)) {}

TestNetworkContextWithHostResolver::~TestNetworkContextWithHostResolver() =
    default;

void TestNetworkContextWithHostResolver::ResolveHost(
    mojom::HostResolverHostPtr host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    mojom::ResolveHostParametersPtr optional_parameters,
    mojo::PendingRemote<network::mojom::ResolveHostClient>
        pending_response_client) {
  ResolveHostImpl(std::move(host), network_anonymization_key,
                  std::move(optional_parameters),
                  std::move(pending_response_client));
}

void TestNetworkContextWithHostResolver::ResolveHostImpl(
    mojom::HostResolverHostPtr host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    mojom::ResolveHostParametersPtr optional_parameters,
    mojo::PendingRemote<network::mojom::ResolveHostClient>
        pending_response_client) {
  DCHECK(host_resolver_);
  mojo::Remote<network::mojom::ResolveHostClient> response_client(
      std::move(pending_response_client));

  auto internal_request =
      host->is_host_port_pair()
          ? host_resolver_->CreateRequest(
                host->get_host_port_pair(), network_anonymization_key,
                net::NetLogWithSource::Make(net::NetLog::Get(),
                                            net::NetLogSourceType::NONE),
                /*optional_parameters=*/std::nullopt)
          : host_resolver_->CreateRequest(
                host->get_scheme_host_port(), network_anonymization_key,
                net::NetLogWithSource::Make(net::NetLog::Get(),
                                            net::NetLogSourceType::NONE),
                /*optional_parameters=*/std::nullopt);

  auto* ptr = internal_request.get();
  auto [async_callback, sync_callback] = base::SplitOnceCallback(
      base::BindOnce(&TestNetworkContextWithHostResolver::OnResolveHostComplete,
                     base::Unretained(this), std::move(response_client),
                     std::move(internal_request)));

  // See ResolveHostRequest::Start() for an explanation why only one callback
  // will be invoked.
  int rv = ptr->Start(std::move(async_callback));
  if (rv != net::ERR_IO_PENDING) {
    std::move(sync_callback).Run(rv);
  }
}

void TestNetworkContextWithHostResolver::OnResolveHostComplete(
    mojo::Remote<mojom::ResolveHostClient> response_client,
    std::unique_ptr<net::HostResolver::ResolveHostRequest> internal_request,
    int error) {
  response_client->OnComplete(
      error, internal_request->GetResolveErrorInfo(),
      base::OptionalFromPtr(internal_request->GetAddressResults()),
      /*endpoint_results_with_metadata=*/std::nullopt);
  response_client.reset();
}

}  // namespace network
