// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_FACTORY_IMPL_H_
#define SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_FACTORY_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace proxy_resolver {

class ProxyResolverV8TracingFactory;

// mojom::ProxyResolverFactory implementation that handles multiple bound pipes.
class ProxyResolverFactoryImpl : public mojom::ProxyResolverFactory {
 public:
  explicit ProxyResolverFactoryImpl(
      mojo::PendingReceiver<mojom::ProxyResolverFactory> receiver);
  ~ProxyResolverFactoryImpl() override;

  // Used by jobs to pass ownership of a newly bound ProxyResolver to this
  // factory.
  void AddResolver(std::unique_ptr<mojom::ProxyResolver> resolver,
                   mojo::PendingReceiver<mojom::ProxyResolver> receiver);

 protected:
  // Visible for tests.
  ProxyResolverFactoryImpl(
      mojo::PendingReceiver<mojom::ProxyResolverFactory> receiver,
      std::unique_ptr<ProxyResolverV8TracingFactory> proxy_resolver_factory);

 private:
  class Job;

  // mojom::ProxyResolverFactory override.
  void CreateResolver(
      const std::string& pac_script,
      mojo::PendingReceiver<mojom::ProxyResolver> receiver,
      mojo::PendingRemote<mojom::ProxyResolverFactoryRequestClient> client)
      override;

  void RemoveJob(Job* job);

  const std::unique_ptr<ProxyResolverV8TracingFactory>
      proxy_resolver_impl_factory_;

  std::map<Job*, std::unique_ptr<Job>> jobs_;

  mojo::Receiver<mojom::ProxyResolverFactory> receiver_;
  mojo::UniqueReceiverSet<mojom::ProxyResolver> resolvers_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryImpl);
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_FACTORY_IMPL_H_
