// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/proxy_server.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "net/base/proxy_string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(ProxyServerTest, DefaultConstructor) {
  ProxyServer proxy_server;
  EXPECT_FALSE(proxy_server.is_valid());
}

TEST(ProxyServerTest, FromSchemeHostAndPort) {
  const struct {
    const ProxyServer::Scheme input_scheme;
    const char* const input_host;
    const std::optional<uint16_t> input_port;
    const char* const input_port_str;
    const char* const expected_host;
    const uint16_t expected_port;
  } tests[] = {
      {ProxyServer::SCHEME_HTTP, "foopy", 80, "80", "foopy", 80},

      // Non-standard port
      {ProxyServer::SCHEME_HTTP, "foopy", 10, "10", "foopy", 10},
      {ProxyServer::SCHEME_HTTP, "foopy", 0, "0", "foopy", 0},

      // Hostname canonicalization
      {ProxyServer::SCHEME_HTTP, "FoOpY", 80, "80", "foopy", 80},
      {ProxyServer::SCHEME_HTTP, "f\u00fcpy", 80, "80", "xn--fpy-hoa", 80},

      // IPv4 literal
      {ProxyServer::SCHEME_HTTP, "1.2.3.4", 80, "80", "1.2.3.4", 80},

      // IPv4 literal canonicalization
      {ProxyServer::SCHEME_HTTP, "127.1", 80, "80", "127.0.0.1", 80},
      {ProxyServer::SCHEME_HTTP, "0x7F.0x1", 80, "80", "127.0.0.1", 80},
      {ProxyServer::SCHEME_HTTP, "0177.01", 80, "80", "127.0.0.1", 80},

      // IPv6 literal
      {ProxyServer::SCHEME_HTTP, "[3ffe:2a00:100:7031::1]", 80, "80",
       "[3ffe:2a00:100:7031::1]", 80},
      {ProxyServer::SCHEME_HTTP, "3ffe:2a00:100:7031::1", 80, "80",
       "[3ffe:2a00:100:7031::1]", 80},

      // IPv6 literal canonicalization
      {ProxyServer::SCHEME_HTTP, "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210", 80,
       "80", "[fedc:ba98:7654:3210:fedc:ba98:7654:3210]", 80},
      {ProxyServer::SCHEME_HTTP, "::192.9.5.5", 80, "80", "[::c009:505]", 80},

      // Other schemes
      {ProxyServer::SCHEME_HTTPS, "foopy", 111, "111", "foopy", 111},
      {ProxyServer::SCHEME_QUIC, "foopy", 111, "111", "foopy", 111},
      {ProxyServer::SCHEME_SOCKS4, "foopy", 111, "111", "foopy", 111},
      {ProxyServer::SCHEME_SOCKS5, "foopy", 111, "111", "foopy", 111},
      {ProxyServer::SCHEME_HTTPS, " foopy \n", 111, "111", "foopy", 111},

      // Default ports
      {ProxyServer::SCHEME_HTTP, "foopy", std::nullopt, "", "foopy", 80},
      {ProxyServer::SCHEME_HTTPS, "foopy", std::nullopt, "", "foopy", 443},
      {ProxyServer::SCHEME_QUIC, "foopy", std::nullopt, "", "foopy", 443},
      {ProxyServer::SCHEME_SOCKS4, "foopy", std::nullopt, "", "foopy", 1080},
      {ProxyServer::SCHEME_SOCKS5, "foopy", std::nullopt, "", "foopy", 1080},
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    SCOPED_TRACE(base::NumberToString(i) + ": " + tests[i].input_host + ":" +
                 base::NumberToString(tests[i].input_port.value_or(-1)));
    auto proxy = ProxyServer::FromSchemeHostAndPort(
        tests[i].input_scheme, tests[i].input_host, tests[i].input_port);

    ASSERT_TRUE(proxy.is_valid());
    EXPECT_EQ(proxy.scheme(), tests[i].input_scheme);
    EXPECT_EQ(proxy.GetHost(), tests[i].expected_host);
    EXPECT_EQ(proxy.GetPort(), tests[i].expected_port);

    auto proxy_from_string_port = ProxyServer::FromSchemeHostAndPort(
        tests[i].input_scheme, tests[i].input_host, tests[i].input_port_str);
    EXPECT_TRUE(proxy_from_string_port.is_valid());
    EXPECT_EQ(proxy, proxy_from_string_port);
  }
}

TEST(ProxyServerTest, InvalidHostname) {
  const char* const tests[]{
      "",
      "[]",
      "[foo]",
      "foo:",
      "foo:80",
      ":",
      "http://foo",
      "3ffe:2a00:100:7031::1]",
      "[3ffe:2a00:100:7031::1",
      "foo.80",
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    SCOPED_TRACE(base::NumberToString(i) + ": " + tests[i]);
    auto proxy = ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP,
                                                    tests[i], 80);
    EXPECT_FALSE(proxy.is_valid());
  }
}

TEST(ProxyServerTest, InvalidPort) {
  const char* const tests[]{
      "-1",
      "65536",
      "foo",
      "0x35",
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    SCOPED_TRACE(base::NumberToString(i) + ": " + tests[i]);
    auto proxy = ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP,
                                                    "foopy", tests[i]);
    EXPECT_FALSE(proxy.is_valid());
  }
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
    auto proxy = ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP,
                                                    "foo", std::nullopt);
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_TRUE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_TRUE(proxy.is_http_like());
    EXPECT_FALSE(proxy.is_secure_http_like());
  }

  // HTTPS proxy.
  {
    auto proxy = ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS,
                                                    "foo", std::nullopt);
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_TRUE(proxy.is_https());
    EXPECT_TRUE(proxy.is_http_like());
    EXPECT_TRUE(proxy.is_secure_http_like());
  }

  // QUIC proxy.
  {
    auto proxy = ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC,
                                                    "foo", std::nullopt);
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_TRUE(proxy.is_http_like());
    EXPECT_TRUE(proxy.is_secure_http_like());
  }

  // SOCKS5 proxy.
  {
    auto proxy = ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_SOCKS5,
                                                    "foo", std::nullopt);
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_FALSE(proxy.is_http_like());
    EXPECT_FALSE(proxy.is_secure_http_like());
  }
}

}  // namespace

}  // namespace net
