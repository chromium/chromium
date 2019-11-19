// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_HOST_RESOLVER_MOJO_H_
#define SERVICES_PROXY_RESOLVER_HOST_RESOLVER_MOJO_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/dns/host_cache.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "services/proxy_resolver/proxy_host_resolver.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace net {
class NetworkIsolationKey;
}  // namespace net

namespace proxy_resolver {

// A ProxyHostResolver implementation that converts requests to mojo types and
// forwards them to a mojo Impl interface.
class HostResolverMojo : public ProxyHostResolver {
 public:
  class Impl {
   public:
    virtual ~Impl() = default;
    virtual void ResolveDns(
        const std::string& hostname,
        net::ProxyResolveDnsOperation operation,
        const net::NetworkIsolationKey& network_isolation_key,
        mojo::PendingRemote<mojom::HostResolverRequestClient> client) = 0;
  };

  // |impl| must outlive |this|.
  explicit HostResolverMojo(Impl* impl);
  ~HostResolverMojo() override;

  // ProxyHostResolver overrides.
  std::unique_ptr<Request> CreateRequest(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkIsolationKey& network_isolation_key) override;

 private:
  class Job;
  class RequestImpl;

  Impl* const impl_;

  std::unique_ptr<net::HostCache> host_cache_;
  base::WeakPtrFactory<net::HostCache> host_cache_weak_factory_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HostResolverMojo);
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_HOST_RESOLVER_MOJO_H_
