// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_param_mojom_traits.h"

#include <vector>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
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

// TODO(crbug.com/365771838): Add tests for non-ip protection nested proxy
// chains if support is enabled for all builds.
TEST(ProxyChain, SerializeAndDeserialize) {
  const net::ProxyChain kChains[] = {
      net::ProxyChain::Direct(),
      net::ProxyChain(net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, "foo1", 80)),
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "foo1", 80),
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "foo2", 80),
      }),
      net::ProxyChain::ForIpProtection({}),
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "foo1", 80),
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "foo2", 80),
      }),
      net::ProxyChain::ForIpProtection(
          {
              net::ProxyServer::FromSchemeHostAndPort(
                  net::ProxyServer::SCHEME_HTTPS, "foo1", 80),
          },
          /*chain_id=*/3),
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

}  // namespace network
