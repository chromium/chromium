// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/cpp/network_param_mojom_traits.h"
#include "services/network/public/mojom/network_param.mojom.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

// TODO(crbug.com/365771838): Add tests for non-ip protection nested proxy
// chains if support is enabled for all builds.
TEST(ProxyInfo, SerializeAndDeserialize) {
  std::vector<net::ProxyInfo> infos;

  net::ProxyInfo direct;
  direct.UseDirect();
  infos.push_back(std::move(direct));

  net::ProxyInfo single_proxy_chain;
  single_proxy_chain.UseProxyChain(net::ProxyChain::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, "foo1", 443));
  infos.push_back(std::move(single_proxy_chain));

  net::ProxyInfo multi_proxy_chain;
  multi_proxy_chain.UseProxyChain(net::ProxyChain::ForIpProtection({
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "foo1", 443),
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "foo2", 443),
  }));
  infos.push_back(std::move(multi_proxy_chain));

  for (const auto& original : infos) {
    // Only the underlying ProxyList gets copied, so only use methods that pull
    // from that here.
    SCOPED_TRACE(original.proxy_chain().ToDebugString());
    EXPECT_TRUE(original.proxy_chain().IsValid());
    net::ProxyInfo copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<proxy_resolver::mojom::ProxyInfo>(
            original, copied));
    EXPECT_TRUE(original.proxy_list().Equals(copied.proxy_list()));
  }
}

}  // namespace network
