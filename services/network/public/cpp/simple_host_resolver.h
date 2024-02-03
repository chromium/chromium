// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_HOST_RESOLVER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_HOST_RESOLVER_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace net {
class AddressList;
}  // namespace net

namespace network {

namespace mojom {
class NetworkContext;
}  // namespace mojom

// Wraps network.mojom.HostResolver and allows it to be used with callbacks
// instead of receivers effectively eliminating the need to create custom
// resolver clients.
//  * With mojom.HostResolver:
//    - Create a resolver client that inherits from mojom.ResolveHostClient
//      and override OnComplete() method;
//    - Call mojom.HostResolver.ResolveHost();
//    - Wait till OnComplete() is invoked and manually delete the client.
//  * With SimpleHostResolver:
//    - Call SimpleHostResolver.ResolveHost() with callback mimicking
//      OnComplete() and wait for it to fire.
//
// Prefer using this class over mojom.HostResolver unless you're interested
// in OnTextResults()/OnHostnameResults() events.
//
// Deleting a SimpleHostResolver cancels outstanding resolve requests.
class COMPONENT_EXPORT(NETWORK_CPP) SimpleHostResolver {
 public:
  using NetworkContextFactory =
      base::RepeatingCallback<mojom::NetworkContext*()>;

  using ResolveHostCallback = base::OnceCallback<void(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const std::optional<net::AddressList>& resolved_addresses,
      const std::optional<net::HostResolverEndpointResults>&
          endpoint_results_with_metadata)>;

  // Creates a SimpleHostResolver using the |network_context_factory|.
  // Use this if the associated network context might change over time (for
  // instance, when getting it from StoragePartition).
  static std::unique_ptr<SimpleHostResolver> Create(
      NetworkContextFactory network_context_factory);

  // Creates a SimpleHostResolver from the given |network_context|.
  // |network_context| must outlive SimpleHostResolver.
  static std::unique_ptr<SimpleHostResolver> Create(
      mojom::NetworkContext* network_context);

  virtual ~SimpleHostResolver() = default;

  SimpleHostResolver(const SimpleHostResolver&) = delete;
  SimpleHostResolver& operator=(const SimpleHostResolver&) = delete;

  // Mimics mojom.HostResolver.ResolveHost(), but dumps the result into the
  // provided |callback| instead of invoking OnComplete() on a resolver client.
  //
  // See mojom.ResolveHostClient.OnComplete() for more information on response
  // format and callback parameter descriptions.
  //
  // It's safe to supply |callback| bound via Unretained() since |callback| can
  // only be run while |this| is alive (destroying |this| cancels all pending
  // callbacks).
  // If mojo pipe breaks |callback| will be invoked with net::ERR_FAILED.
  virtual void ResolveHost(
      mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojom::ResolveHostParametersPtr optional_parameters,
      ResolveHostCallback callback) = 0;

  // Tells how many requests there are in flight.
  virtual uint32_t GetNumOutstandingRequestsForTesting() const = 0;

 protected:
  SimpleHostResolver() = default;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_HOST_RESOLVER_H_
