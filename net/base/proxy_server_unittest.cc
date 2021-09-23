// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_server.h"

#include "base/cxx17_backports.h"
#include "net/base/proxy_string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(ProxyServerTest, DefaultConstructor) {
  ProxyServer proxy_server;
  EXPECT_FALSE(proxy_server.is_valid());
}

TEST(ProxyServerTest, ComparatorAndEquality) {
  const struct {
    // Inputs.
    ProxyServer server1;
    ProxyServer server2;

    // Expectation.
    //   -1 means server1 is less than server2
    //    0 means server1 equals server2
    //    1 means server1 is greater than server2
    int expected_comparison;
  } kTests[] = {
      {// Equal.
       ProxyUriToProxyServer("foo:11", ProxyServer::SCHEME_HTTP),
       ProxyUriToProxyServer("http://foo:11", ProxyServer::SCHEME_HTTP), 0},
      {// Port is different.
       ProxyUriToProxyServer("foo:333", ProxyServer::SCHEME_HTTP),
       ProxyUriToProxyServer("foo:444", ProxyServer::SCHEME_HTTP), -1},
      {// Host is different.
       ProxyUriToProxyServer("foo:33", ProxyServer::SCHEME_HTTP),
       ProxyUriToProxyServer("bar:33", ProxyServer::SCHEME_HTTP), 1},
      {// Scheme is different.
       ProxyUriToProxyServer("socks4://foo:33", ProxyServer::SCHEME_HTTP),
       ProxyUriToProxyServer("http://foo:33", ProxyServer::SCHEME_HTTP), 1},
  };

  for (const auto& test : kTests) {
    EXPECT_TRUE(test.server1.is_valid());
    EXPECT_TRUE(test.server2.is_valid());

    switch (test.expected_comparison) {
      case -1:
        EXPECT_TRUE(test.server1 < test.server2);
        EXPECT_FALSE(test.server2 < test.server1);
        EXPECT_FALSE(test.server2 == test.server1);
        EXPECT_FALSE(test.server1 == test.server2);
        break;
      case 0:
        EXPECT_FALSE(test.server1 < test.server2);
        EXPECT_FALSE(test.server2 < test.server1);
        EXPECT_TRUE(test.server2 == test.server1);
        EXPECT_TRUE(test.server1 == test.server2);
        break;
      case 1:
        EXPECT_FALSE(test.server1 < test.server2);
        EXPECT_TRUE(test.server2 < test.server1);
        EXPECT_FALSE(test.server2 == test.server1);
        EXPECT_FALSE(test.server1 == test.server2);
        break;
      default:
        FAIL() << "Invalid expectation. Can be only -1, 0, 1";
    }
  }
}

// Tests the various "is_*()" methods on ProxyServer.
TEST(ProxyServerTest, Properties) {
  // HTTP proxy.
  {
    auto proxy = PacResultElementToProxyServer("PROXY foo");
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_TRUE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_TRUE(proxy.is_http_like());
    EXPECT_FALSE(proxy.is_secure_http_like());
  }

  // HTTPS proxy.
  {
    auto proxy = PacResultElementToProxyServer("HTTPS foo");
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_TRUE(proxy.is_https());
    EXPECT_TRUE(proxy.is_http_like());
    EXPECT_TRUE(proxy.is_secure_http_like());
  }

  // QUIC proxy.
  {
    auto proxy = PacResultElementToProxyServer("QUIC foo");
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_TRUE(proxy.is_http_like());
    EXPECT_TRUE(proxy.is_secure_http_like());
  }

  // SOCKS5 proxy.
  {
    auto proxy = PacResultElementToProxyServer("SOCKS5 foo");
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_FALSE(proxy.is_http_like());
    EXPECT_FALSE(proxy.is_secure_http_like());
  }

  // DIRECT
  {
    auto proxy = PacResultElementToProxyServer("DIRECT");
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_FALSE(proxy.is_http_like());
    EXPECT_FALSE(proxy.is_secure_http_like());
  }
}

}  // namespace

}  // namespace net
