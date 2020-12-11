// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_HOST_RESOLVER_H_
#define SERVICES_NETWORK_HOST_RESOLVER_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/dns/public/dns_query_type.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace net {
class HostResolver;
class HostPortPair;
class NetLog;
class NetworkIsolationKey;
}  // namespace net

namespace network {
class HostResolverMdnsListener;
class ResolveHostRequest;

class COMPONENT_EXPORT(NETWORK_SERVICE) HostResolver
    : public mojom::HostResolver {
 public:
  using ConnectionShutdownCallback = base::OnceCallback<void(HostResolver*)>;

  // Constructs and binds to the given mojom::HostResolver pipe. On pipe close,
  // cancels all outstanding receivers (whether made through the pipe or by
  // directly calling ResolveHost()) with ERR_FAILED. Also on pipe close, calls
  // |connection_shutdown_callback| and passes |this| to notify that the
  // resolver has cancelled all receivers and may be cleaned up.
  HostResolver(mojo::PendingReceiver<mojom::HostResolver> resolver_receiver,
               ConnectionShutdownCallback connection_shutdown_callback,
               net::HostResolver* internal_resolver,
               net::NetLog* net_log);
  // Constructor for when the resolver will not be bound to a
  // mojom::HostResolver pipe, eg because it is handling ResolveHost requests
  // made directly on NetworkContext.
  HostResolver(net::HostResolver* internal_resolver, net::NetLog* net_log);
  ~HostResolver() override;

  void ResolveHost(
      const net::HostPortPair& host,
      const net::NetworkIsolationKey& network_isolation_key,
      mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<mojom::ResolveHostClient> response_client) override;
  void MdnsListen(const net::HostPortPair& host,
                  net::DnsQueryType query_type,
                  mojo::PendingRemote<mojom::MdnsListenClient> response_client,
                  MdnsListenCallback callback) override;

  size_t GetNumOutstandingRequestsForTesting() const;

  // Sets a global callback when a ResolveHost call arrives.
  using ResolveHostCallback =
      base::RepeatingCallback<void(const std::string& host)>;
  static void SetResolveHostCallbackForTesting(ResolveHostCallback callback);

 private:
  void AsyncSetUp();
  void OnResolveHostComplete(ResolveHostRequest* request, int error);
  void OnMdnsListenerCancelled(HostResolverMdnsListener* listener);
  void OnConnectionError();

  mojo::Receiver<mojom::HostResolver> receiver_;
  mojo::PendingReceiver<mojom::HostResolver> pending_receiver_;
  ConnectionShutdownCallback connection_shutdown_callback_;
  std::set<std::unique_ptr<ResolveHostRequest>, base::UniquePtrComparator>
      requests_;
  std::set<std::unique_ptr<HostResolverMdnsListener>, base::UniquePtrComparator>
      listeners_;

  net::HostResolver* const internal_resolver_;
  net::NetLog* const net_log_;

  base::WeakPtrFactory<HostResolver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HostResolver);
};

}  // namespace network

#endif  // SERVICES_NETWORK_HOST_RESOLVER_H_
