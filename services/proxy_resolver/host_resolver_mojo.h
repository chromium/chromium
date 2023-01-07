// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_HOST_RESOLVER_MOJO_H_
#define SERVICES_PROXY_RESOLVER_HOST_RESOLVER_MOJO_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "services/proxy_resolver/proxy_host_resolver.h"
#include "services/proxy_resolver/proxy_host_resolver_cache.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace net {
class NetworkAnonymizationKey;
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
        const net::NetworkAnonymizationKey& network_anonymization_key,
        mojo::PendingRemote<mojom::HostResolverRequestClient> client) = 0;
  };

  // |impl| must outlive |this|.
  explicit HostResolverMojo(Impl* impl);

  HostResolverMojo(const HostResolverMojo&) = delete;
  HostResolverMojo& operator=(const HostResolverMojo&) = delete;

  ~HostResolverMojo() override;

  // ProxyHostResolver overrides.
  std::unique_ptr<Request> CreateRequest(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkAnonymizationKey& network_anonymization_key) override;

 private:
  class Job;
  class RequestImpl;

  const raw_ptr<Impl> impl_;

  ProxyHostResolverCache host_cache_;
  base::WeakPtrFactory<ProxyHostResolverCache> host_cache_weak_factory_{
      &host_cache_};

  base::ThreadChecker thread_checker_;
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_HOST_RESOLVER_MOJO_H_
