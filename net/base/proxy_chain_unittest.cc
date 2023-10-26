// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_chain.h"

#include <sstream>

#include "base/strings/string_number_conversions.h"
#include "net/base/proxy_string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {

TEST(ProxyChainTest, DefaultConstructor) {
  ProxyChain proxy_chain;
  EXPECT_FALSE(proxy_chain.proxy_server().is_valid());
}

TEST(ProxyChainTest, Ostream) {
  ProxyChain proxy_chain =
      ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP, "foo", 80);
  std::ostringstream out;
  out << proxy_chain;
  EXPECT_EQ(out.str(), "PROXY foo:80");
}

TEST(ProxyChainTest, FromSchemeHostAndPort) {
  const struct {
    const ProxyServer::Scheme input_scheme;
    const char* const input_host;
    const absl::optional<uint16_t> input_port;
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

      // Default ports
      {ProxyServer::SCHEME_HTTP, "foopy", absl::nullopt, "", "foopy", 80},
      {ProxyServer::SCHEME_HTTPS, "foopy", absl::nullopt, "", "foopy", 443},
      {ProxyServer::SCHEME_QUIC, "foopy", absl::nullopt, "", "foopy", 443},
      {ProxyServer::SCHEME_SOCKS4, "foopy", absl::nullopt, "", "foopy", 1080},
      {ProxyServer::SCHEME_SOCKS5, "foopy", absl::nullopt, "", "foopy", 1080},
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    SCOPED_TRACE(base::NumberToString(i) + ": " + tests[i].input_host + ":" +
                 base::NumberToString(tests[i].input_port.value_or(-1)));
    auto chain = ProxyChain::FromSchemeHostAndPort(
        tests[i].input_scheme, tests[i].input_host, tests[i].input_port);
    auto proxy = chain.proxy_server();

    ASSERT_TRUE(proxy.is_valid());
    EXPECT_EQ(proxy.scheme(), tests[i].input_scheme);
    EXPECT_EQ(proxy.GetHost(), tests[i].expected_host);
    EXPECT_EQ(proxy.GetPort(), tests[i].expected_port);

    auto chain_from_string_port = ProxyChain::FromSchemeHostAndPort(
        tests[i].input_scheme, tests[i].input_host, tests[i].input_port_str);
    auto proxy_from_string_port = chain_from_string_port.proxy_server();
    EXPECT_TRUE(proxy_from_string_port.is_valid());
    EXPECT_EQ(proxy, proxy_from_string_port);
  }
}

TEST(ProxyChainTest, InvalidHostname) {
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
    auto proxy = ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP,
                                                   tests[i], 80);
    EXPECT_FALSE(proxy.proxy_server().is_valid());
  }
}

TEST(ProxyChainTest, InvalidPort) {
  const char* const tests[]{
      "-1",
      "65536",
      "foo",
      "0x35",
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    SCOPED_TRACE(base::NumberToString(i) + ": " + tests[i]);
    auto proxy = ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP,
                                                   "foopy", tests[i]);
    EXPECT_FALSE(proxy.proxy_server().is_valid());
  }
}

TEST(ProxyChainTest, ComparatorAndEquality) {
  const struct {
    // Inputs.
    ProxyChain chain1;
    ProxyChain chain2;

    // Expectation.
    //   -1 means chain1 is less than chain2
    //    0 means chain1 equals chain2
    //    1 means chain1 is greater than chain2
    int expected_comparison;
  } kTests[] = {
      {// Equal.
       ProxyUriToProxyChain("foo:11", ProxyServer::SCHEME_HTTP),
       ProxyUriToProxyChain("http://foo:11", ProxyServer::SCHEME_HTTP), 0},
      {// Port is different.
       ProxyUriToProxyChain("foo:333", ProxyServer::SCHEME_HTTP),
       ProxyUriToProxyChain("foo:444", ProxyServer::SCHEME_HTTP), -1},
      {// Host is different.
       ProxyUriToProxyChain("foo:33", ProxyServer::SCHEME_HTTP),
       ProxyUriToProxyChain("bar:33", ProxyServer::SCHEME_HTTP), 1},
      {// Scheme is different.
       ProxyUriToProxyChain("socks4://foo:33", ProxyServer::SCHEME_HTTP),
       ProxyUriToProxyChain("http://foo:33", ProxyServer::SCHEME_HTTP), 1},
  };

  for (const auto& test : kTests) {
    EXPECT_TRUE(test.chain1.proxy_server().is_valid());
    EXPECT_TRUE(test.chain2.proxy_server().is_valid());

    switch (test.expected_comparison) {
      case -1:
        EXPECT_TRUE(test.chain1 < test.chain2);
        EXPECT_FALSE(test.chain2 < test.chain1);
        EXPECT_FALSE(test.chain2 == test.chain1);
        EXPECT_FALSE(test.chain1 == test.chain2);
        break;
      case 0:
        EXPECT_FALSE(test.chain1 < test.chain2);
        EXPECT_FALSE(test.chain2 < test.chain1);
        EXPECT_TRUE(test.chain2 == test.chain1);
        EXPECT_TRUE(test.chain1 == test.chain2);
        break;
      case 1:
        EXPECT_FALSE(test.chain1 < test.chain2);
        EXPECT_TRUE(test.chain2 < test.chain1);
        EXPECT_FALSE(test.chain2 == test.chain1);
        EXPECT_FALSE(test.chain1 == test.chain2);
        break;
      default:
        FAIL() << "Invalid expectation. Can be only -1, 0, 1";
    }
  }
}

}  // namespace

}  // namespace net
