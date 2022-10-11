// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_MOJO_HOST_RESOLVER_IMPL_H_
#define SERVICES_NETWORK_MOJO_HOST_RESOLVER_IMPL_H_

#include <list>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/log/net_log_with_source.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace net {
class HostResolver;
class NetworkAnonymizationKey;
}  // namespace net

namespace network {

// MojoHostResolverImpl handles mojo host resolution requests from the Proxy
// Resolver Service. Inbound Mojo requests are sent to the HostResolver passed
// into the constructor. When destroyed, any outstanding resolver requests are
// cancelled. If a request's HostResolverRequestClient is shut down, the
// associated resolver request is cancelled.
//
// TODO(mmenke): Rename this to something that makes it clearer that this is
// just for use by the ProxyResolverFactoryMojo, or merge it into
// ProxyResolverFactoryMojo.
class COMPONENT_EXPORT(NETWORK_SERVICE) MojoHostResolverImpl {
 public:
  // |resolver| is expected to outlive |this|.
  MojoHostResolverImpl(net::HostResolver* resolver,
                       const net::NetLogWithSource& net_log);

  MojoHostResolverImpl(const MojoHostResolverImpl&) = delete;
  MojoHostResolverImpl& operator=(const MojoHostResolverImpl&) = delete;

  ~MojoHostResolverImpl();

  void Resolve(
      const std::string& hostname,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      bool is_ex,
      mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
          client);

  bool request_in_progress() { return !pending_jobs_.empty(); }

 private:
  class Job;

  // Removes |job| from the set of pending jobs.
  void DeleteJob(std::list<Job>::iterator job);

  // Resolver for resolving incoming requests. Not owned.
  raw_ptr<net::HostResolver> resolver_;

  // The NetLogWithSource to be passed to |resolver_| for all requests.
  const net::NetLogWithSource net_log_;

  // All pending jobs, so they can be cancelled when this service is destroyed.
  std::list<Job> pending_jobs_;

  base::ThreadChecker thread_checker_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_MOJO_HOST_RESOLVER_IMPL_H_
