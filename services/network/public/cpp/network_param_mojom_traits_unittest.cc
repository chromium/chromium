// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_param_mojom_traits.h"

#include <vector>

#include "build/buildflag.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/net_buildflags.h"
#include "services/network/public/mojom/network_param.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(ProxyChain, SerializeAndDeserializeInvalid) {
  net::ProxyChain original = net::ProxyChain();
  net::ProxyChain copied;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::ProxyChain>(
      original, copied));
  EXPECT_EQ(original, copied);
}

TEST(ProxyChain, SerializeAndDeserialize) {
  const net::ProxyChain kChains[] = {
      net::ProxyChain::Direct(),
      net::ProxyChain(net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, "foo1", 80)),
#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
      net::ProxyChain({
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "foo1", 80),
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "foo2", 80),
      }),
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
  };
  for (auto& original : kChains) {
    SCOPED_TRACE(original.ToDebugString());
    EXPECT_TRUE(original.IsValid());
    net::ProxyChain copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::ProxyChain>(
        original, copied));
    EXPECT_EQ(original, copied);
  }
}

// Ensure that attempting to deserialize a proxy chain with an invalid state
// fails gracefully.
TEST(ProxyChain, DeserializeProxyChainWithInvalidState) {
  // Construct a network::mojom::ProxyChain with an invalid state (one that
  // could not have been created by serialization of a net::ProxyChain).
  network::mojom::ProxyChainPtr mojom_chain = network::mojom::ProxyChain::New();
  std::vector<net::ProxyServer> proxy_servers{net::ProxyServer()};
  mojom_chain->proxy_servers = std::make_optional(std::move(proxy_servers));

  net::ProxyChain copied;
  // Deserialization should fail gracefully without crashing.
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<network::mojom::ProxyChain>(
      mojom_chain, copied));
}

}  // namespace network
