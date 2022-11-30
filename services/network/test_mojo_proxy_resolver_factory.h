// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_MOJO_PROXY_RESOLVER_FACTORY_H_
#define SERVICES_NETWORK_TEST_MOJO_PROXY_RESOLVER_FACTORY_H_

#include <memory>
#include <string>

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/proxy_resolver/proxy_resolver_factory_impl.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace network {

// MojoProxyResolverFactory that runs PAC scripts in-process, for tests.
class TestMojoProxyResolverFactory
    : public proxy_resolver::mojom::ProxyResolverFactory {
 public:
  TestMojoProxyResolverFactory();

  TestMojoProxyResolverFactory(const TestMojoProxyResolverFactory&) = delete;
  TestMojoProxyResolverFactory& operator=(const TestMojoProxyResolverFactory&) =
      delete;

  ~TestMojoProxyResolverFactory() override;

  // Returns true if CreateResolver was called.
  bool resolver_created() const { return resolver_created_; }

  mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
  CreateFactoryRemote();

  // Overridden from interfaces::ProxyResolverFactory:
  void CreateResolver(
      const std::string& pac_script,
      mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
      mojo::PendingRemote<
          proxy_resolver::mojom::ProxyResolverFactoryRequestClient> client)
      override;

 private:
  mojo::Remote<proxy_resolver::mojom::ProxyResolverFactory> factory_;
  proxy_resolver::ProxyResolverFactoryImpl proxy_resolver_factory_impl_;

  mojo::Receiver<ProxyResolverFactory> receiver_{this};

  bool resolver_created_ = false;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_MOJO_PROXY_RESOLVER_FACTORY_H_
