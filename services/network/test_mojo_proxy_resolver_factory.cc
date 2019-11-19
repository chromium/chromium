// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test_mojo_proxy_resolver_factory.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"

namespace network {

TestMojoProxyResolverFactory::TestMojoProxyResolverFactory()
    : proxy_resolver_factory_impl_(factory_.BindNewPipeAndPassReceiver()) {}

TestMojoProxyResolverFactory::~TestMojoProxyResolverFactory() = default;

void TestMojoProxyResolverFactory::CreateResolver(
    const std::string& pac_script,
    mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
    mojo::PendingRemote<
        proxy_resolver::mojom::ProxyResolverFactoryRequestClient> client) {
  resolver_created_ = true;
  factory_->CreateResolver(pac_script, std::move(receiver), std::move(client));
}

mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
TestMojoProxyResolverFactory::CreateFactoryRemote() {
  DCHECK(!receiver_.is_bound());
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace network
