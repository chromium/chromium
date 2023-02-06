// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_WITH_HOST_RESOLVER_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_WITH_HOST_RESOLVER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/host_resolver.h"
#include "services/network/test/test_network_context.h"

namespace network {

// An extension of TextNetworkContext with built-in host resolution routine.
class TestNetworkContextWithHostResolver : public TestNetworkContext {
 public:
  explicit TestNetworkContextWithHostResolver(
      std::unique_ptr<net::HostResolver> host_resolver);
  ~TestNetworkContextWithHostResolver() override;

  // TestNetworkContext:
  void ResolveHost(
      mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient>
          pending_response_client) override;

  net::HostResolver* host_resolver() const { return host_resolver_.get(); }

 protected:
  // This function is factored out so that overrides can inject additional
  // conditions into ResolveHost() without having to rewrite the actual logic.
  // Ignores |optional_parameters|.
  void ResolveHostImpl(
      mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient>
          pending_response_client);

  virtual void OnResolveHostComplete(
      mojo::Remote<mojom::ResolveHostClient> response_client,
      std::unique_ptr<net::HostResolver::ResolveHostRequest> internal_request,
      int error);

 private:
  std::unique_ptr<net::HostResolver> host_resolver_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_WITH_HOST_RESOLVER_H_
