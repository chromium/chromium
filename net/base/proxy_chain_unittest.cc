// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/proxy_chain.h"

#include <optional>
#include <sstream>

#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "build/buildflag.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/net_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(ProxyChainTest, DefaultConstructor) {
  ProxyChain proxy_chain;
  EXPECT_FALSE(proxy_chain.IsValid());
}

TEST(ProxyChainTest, ConstructorsAndAssignmentOperators) {
  std::vector proxy_servers = {
      ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS),
      ProxyUriToProxyServer("foo:666", ProxyServer::SCHEME_HTTPS)};

  ProxyChain proxy_chain = ProxyChain(proxy_servers);

  ProxyChain copy_constructed(proxy_chain);
  EXPECT_EQ(proxy_chain, copy_constructed);

  ProxyChain copy_assigned = proxy_chain;
  EXPECT_EQ(proxy_chain, copy_assigned);

  ProxyChain move_constructed{std::move(copy_constructed)};
  EXPECT_EQ(proxy_chain, move_constructed);

  ProxyChain move_assigned = std::move(copy_assigned);
  EXPECT_EQ(proxy_chain, move_assigned);
}

TEST(ProxyChainTest, DirectProxy) {
  ProxyChain proxy_chain1 = ProxyChain::Direct();
  ProxyChain proxy_chain2 = ProxyChain(std::vector<ProxyServer>());
  std::vector<ProxyServer> proxy_servers = {};

  // Equal and valid proxy chains.
  ASSERT_EQ(proxy_chain1, proxy_chain2);
  EXPECT_TRUE(proxy_chain1.IsValid());
  EXPECT_TRUE(proxy_chain2.IsValid());

  EXPECT_TRUE(proxy_chain1.is_direct());
  EXPECT_FALSE(proxy_chain1.is_single_proxy());
  EXPECT_FALSE(proxy_chain1.is_multi_proxy());
  ASSERT_EQ(proxy_chain1.length(), 0u);
  ASSERT_EQ(proxy_chain1.proxy_servers(), proxy_servers);
}

TEST(ProxyChainTest, Ostream) {
  ProxyChain proxy_chain =
      ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP, "foo", 80);
  std::ostringstream out;
  out << proxy_chain;
  EXPECT_EQ(out.str(), "[foo:80]");
}

TEST(ProxyChainTest, ToDebugString) {
  ProxyChain proxy_chain1 =
      ProxyChain(ProxyUriToProxyServer("foo:333", ProxyServer::SCHEME_SOCKS5));
  EXPECT_EQ(proxy_chain1.ToDebugString(), "[socks5://foo:333]");

  ProxyChain direct_proxy_chain = ProxyChain::Direct();
  EXPECT_EQ(direct_proxy_chain.ToDebugString(), "[direct://]");

  ProxyChain ip_protection_proxy_chain = ProxyChain::ForIpProtection(
      {ProxyUriToProxyServer("foo:444", ProxyServer::SCHEME_HTTPS),
       ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS)});
  EXPECT_EQ(ip_protection_proxy_chain.ToDebugString(),
            "[https://foo:444, https://foo:555] (IP Protection)");

  ProxyChain invalid_proxy_chain = ProxyChain();
  EXPECT_EQ(invalid_proxy_chain.ToDebugString(), "INVALID PROXY CHAIN");

// Multi-proxy chains can only be created outside of Ip Protection in debug
// builds.
#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
  ProxyChain proxy_chain2 =
      ProxyChain({ProxyUriToProxyServer("foo:444", ProxyServer::SCHEME_HTTPS),
                  ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS)});
  EXPECT_EQ(proxy_chain2.ToDebugString(), "[https://foo:444, https://foo:555]");
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
}

TEST(ProxyChainTest, FromSchemeHostAndPort) {
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
      {ProxyServer::SCHEME_SOCKS4, "foopy", 111, "111", "foopy", 111},
      {ProxyServer::SCHEME_SOCKS5, "foopy", 111, "111", "foopy", 111},

      // Default ports
      {ProxyServer::SCHEME_HTTP, "foopy", std::nullopt, "", "foopy", 80},
      {ProxyServer::SCHEME_HTTPS, "foopy", std::nullopt, "", "foopy", 443},
      {ProxyServer::SCHEME_SOCKS4, "foopy", std::nullopt, "", "foopy", 1080},
      {ProxyServer::SCHEME_SOCKS5, "foopy", std::nullopt, "", "foopy", 1080},
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    SCOPED_TRACE(base::NumberToString(i) + ": " + tests[i].input_host + ":" +
                 base::NumberToString(tests[i].input_port.value_or(-1)));
    auto chain = ProxyChain::FromSchemeHostAndPort(
        tests[i].input_scheme, tests[i].input_host, tests[i].input_port);
    auto proxy = chain.First();

    ASSERT_TRUE(proxy.is_valid());
    EXPECT_EQ(proxy.scheme(), tests[i].input_scheme);
    EXPECT_EQ(proxy.GetHost(), tests[i].expected_host);
    EXPECT_EQ(proxy.GetPort(), tests[i].expected_port);

    auto chain_from_string_port = ProxyChain::FromSchemeHostAndPort(
        tests[i].input_scheme, tests[i].input_host, tests[i].input_port_str);
    auto proxy_from_string_port = chain_from_string_port.First();
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
    EXPECT_FALSE(proxy.IsValid());
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
    EXPECT_FALSE(proxy.IsValid());
  }
}

TEST(ProxyChainTest, SingleProxyChain) {
  auto proxy_server =
      ProxyUriToProxyServer("foo:333", ProxyServer::SCHEME_HTTPS);

  std::vector<ProxyServer> proxy_servers = {proxy_server};
  auto proxy = ProxyChain(proxy_servers);

  EXPECT_FALSE(proxy.is_direct());
  EXPECT_TRUE(proxy.is_single_proxy());
  EXPECT_FALSE(proxy.is_multi_proxy());
  ASSERT_EQ(proxy.proxy_servers(), proxy_servers);
  ASSERT_EQ(proxy.length(), 1u);
  ASSERT_EQ(proxy.GetProxyServer(0), proxy_server);
}

TEST(ProxyChainTest, SplitLast) {
  auto proxy_server1 =
      ProxyUriToProxyServer("foo:333", ProxyServer::SCHEME_HTTPS);
  auto proxy_server2 =
      ProxyUriToProxyServer("foo:444", ProxyServer::SCHEME_HTTPS);
  auto proxy_server3 =
      ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS);

  auto chain3 = ProxyChain::ForIpProtection(
      {proxy_server1, proxy_server2, proxy_server3});
  EXPECT_EQ(chain3.SplitLast(),
            std::make_pair(
                ProxyChain::ForIpProtection({proxy_server1, proxy_server2}),
                proxy_server3));

  auto chain1 = ProxyChain({proxy_server1});
  EXPECT_EQ(chain1.SplitLast(),
            std::make_pair(ProxyChain::Direct(), proxy_server1));

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
  // Multi-proxy chains (not for Ip Protection) are only valid in debug builds.
  auto chain2 = ProxyChain({proxy_server1, proxy_server2});
  EXPECT_EQ(chain2.SplitLast(),
            std::make_pair(ProxyChain({proxy_server1}), proxy_server2));
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
}

TEST(ProxyChainTest, Prefix) {
  auto proxy_server1 =
      ProxyUriToProxyServer("foo:333", ProxyServer::SCHEME_HTTPS);
  auto proxy_server2 =
      ProxyUriToProxyServer("foo:444", ProxyServer::SCHEME_HTTPS);
  auto proxy_server3 =
      ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS);
  auto chain = ProxyChain::ForIpProtection(
      {proxy_server1, proxy_server2, proxy_server3}, /*chain_id=*/2);
  EXPECT_EQ(chain.Prefix(0), ProxyChain::ForIpProtection({}, /*chain_id=*/2));
  EXPECT_EQ(chain.Prefix(1),
            ProxyChain::ForIpProtection({proxy_server1}, /*chain_id=*/2));
  EXPECT_EQ(chain.Prefix(2),
            ProxyChain::ForIpProtection({proxy_server1, proxy_server2},
                                        /*chain_id=*/2));
  EXPECT_EQ(chain.Prefix(3),
            ProxyChain::ForIpProtection(
                {proxy_server1, proxy_server2, proxy_server3}, /*chain_id=*/2));
}

TEST(ProxyChainTest, First) {
  auto proxy_server1 =
      ProxyUriToProxyServer("foo:333", ProxyServer::SCHEME_HTTPS);

  auto chain = ProxyChain({proxy_server1});
  EXPECT_EQ(chain.First(), proxy_server1);

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
  // Multi-proxy chains (not for Ip Protection) are only valid in debug builds.
  auto proxy_server2 =
      ProxyUriToProxyServer("foo:444", ProxyServer::SCHEME_HTTPS);

  chain = ProxyChain({proxy_server1, proxy_server2});
  EXPECT_EQ(chain.First(), proxy_server1);
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
}

TEST(ProxyChainTest, Last) {
  auto proxy_server1 =
      ProxyUriToProxyServer("foo:333", ProxyServer::SCHEME_HTTPS);

  auto chain = ProxyChain({proxy_server1});
  EXPECT_EQ(chain.Last(), proxy_server1);

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
  // Multi-proxy chains (not for Ip Protection) are only valid in debug builds.
  auto proxy_server2 =
      ProxyUriToProxyServer("foo:444", ProxyServer::SCHEME_HTTPS);

  chain = ProxyChain({proxy_server1, proxy_server2});
  EXPECT_EQ(chain.Last(), proxy_server2);
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
}

TEST(ProxyChainTest, IsForIpProtection) {
  auto regular_proxy_chain1 = ProxyChain::Direct();
  EXPECT_FALSE(regular_proxy_chain1.is_for_ip_protection());

  auto ip_protection_proxy_chain1 =
      ProxyChain::ForIpProtection(std::vector<ProxyServer>());
  EXPECT_TRUE(ip_protection_proxy_chain1.is_for_ip_protection());

  auto regular_proxy_chain2 =
      ProxyChain({ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS),
                  ProxyUriToProxyServer("foo:666", ProxyServer::SCHEME_HTTPS)});
  EXPECT_FALSE(regular_proxy_chain2.is_for_ip_protection());

  auto ip_protection_proxy_chain2 = ProxyChain::ForIpProtection(
      {ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS),
       ProxyUriToProxyServer("foo:666", ProxyServer::SCHEME_HTTPS)});
  EXPECT_TRUE(ip_protection_proxy_chain2.is_for_ip_protection());
}

TEST(ProxyChainTest, ForIpProtection) {
  auto ip_protection_proxy_chain1 =
      ProxyChain::ForIpProtection(std::vector<ProxyServer>());
  EXPECT_TRUE(ip_protection_proxy_chain1.is_direct());
  EXPECT_TRUE(ip_protection_proxy_chain1.is_for_ip_protection());
  EXPECT_EQ(ip_protection_proxy_chain1.ip_protection_chain_id(),
            ProxyChain::kDefaultIpProtectionChainId);

  // Ensure that ProxyChain can be reassigned a new value created using its own
  // `proxy_severs()`.
  auto proxy_chain =
      ProxyChain({ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS)});
  auto copied_proxy_chain = proxy_chain;

  // Assert that the newly created `ProxyChain` is not for IP protection.
  EXPECT_FALSE(proxy_chain.is_for_ip_protection());
  EXPECT_EQ(proxy_chain.ip_protection_chain_id(),
            ProxyChain::kNotIpProtectionChainId);

  // Re-assign new value to `proxy_chain` by using its own proxy servers to
  // create a proxy chain for IP protection.
  proxy_chain =
      ProxyChain::ForIpProtection(std::move(proxy_chain.proxy_servers()));

  // Assert re-assigned proxy chain is now for IP protection and contains the
  // same servers from the original copy.
  EXPECT_TRUE(proxy_chain.is_for_ip_protection());
  EXPECT_EQ(proxy_chain.ip_protection_chain_id(),
            ProxyChain::kDefaultIpProtectionChainId);
  EXPECT_FALSE(copied_proxy_chain.is_for_ip_protection());
  EXPECT_EQ(proxy_chain.proxy_servers(), copied_proxy_chain.proxy_servers());

  auto chain_with_id = ProxyChain::ForIpProtection(
      {ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS),
       ProxyUriToProxyServer("foo:666", ProxyServer::SCHEME_HTTPS)},
      /*chain_id=*/3);
  EXPECT_FALSE(chain_with_id.is_direct());
  EXPECT_TRUE(chain_with_id.is_for_ip_protection());
  EXPECT_EQ(chain_with_id.ip_protection_chain_id(), 3);
}

TEST(ProxyChainTest, IsGetToProxyAllowed) {
  auto https_server1 =
      ProxyUriToProxyServer("foo:333", ProxyServer::SCHEME_HTTPS);
  auto https_server2 =
      ProxyUriToProxyServer("foo:444", ProxyServer::SCHEME_HTTPS);
  auto http_server = ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTP);
  auto socks_server =
      ProxyUriToProxyServer("foo:666", ProxyServer::SCHEME_SOCKS4);

  EXPECT_FALSE(ProxyChain::Direct().is_get_to_proxy_allowed());
  EXPECT_TRUE(ProxyChain({https_server1}).is_get_to_proxy_allowed());
  EXPECT_TRUE(ProxyChain({http_server}).is_get_to_proxy_allowed());
  EXPECT_FALSE(ProxyChain({socks_server}).is_get_to_proxy_allowed());
  EXPECT_FALSE(
      ProxyChain({https_server1, https_server2}).is_get_to_proxy_allowed());
}

TEST(ProxyChainTest, IsValid) {
  // Single hop proxy of type Direct is valid.
  EXPECT_TRUE(ProxyChain::Direct().IsValid());

  auto https1 = ProxyUriToProxyServer("foo:444", ProxyServer::SCHEME_HTTPS);
  auto https2 = ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS);
  auto quic1 = ProxyUriToProxyServer("foo:666", ProxyServer::SCHEME_QUIC);
  auto quic2 = ProxyUriToProxyServer("foo:777", ProxyServer::SCHEME_QUIC);
  auto socks = ProxyUriToProxyServer("foo:777", ProxyServer::SCHEME_SOCKS5);

  // Single proxy chain is valid.
  EXPECT_TRUE(ProxyChain({https1}).IsValid());

  // Invalid Chains.
  //
  // If multi-proxy chain support is disabled, any chain greater
  // than length 1 is considered invalid. If multi-proxy support is enabled AND
  // QUIC proxy support is enabled, these chains remain invalid due to the
  // sequence of schemes.
  EXPECT_FALSE(ProxyChain({https1, quic2}).IsValid());
  EXPECT_FALSE(ProxyChain({https1, https2, quic1, quic2}).IsValid());
  // ProxyChain cannot contains socks server. Only QUIC and HTTPS.
  EXPECT_FALSE(ProxyChain({socks, https1}).IsValid());
  EXPECT_FALSE(ProxyChain({socks, https1, https2}).IsValid());
  EXPECT_FALSE(ProxyChain({https1, socks}).IsValid());
  EXPECT_FALSE(ProxyChain({https1, https2, socks}).IsValid());

  // IP protection accepts chains with SCHEME_QUIC and/or multi-proxy chains
  EXPECT_TRUE(ProxyChain::ForIpProtection({https1}).IsValid());
  EXPECT_TRUE(ProxyChain::ForIpProtection({quic1}).IsValid());
  EXPECT_TRUE(ProxyChain::ForIpProtection({https1, https2}).IsValid());
  EXPECT_TRUE(ProxyChain::ForIpProtection({quic1, https1}).IsValid());
  EXPECT_TRUE(
      ProxyChain::ForIpProtection({quic1, quic2, https1, https2}).IsValid());

  // IP protection CHECKs on failure instead of just creating an invalid chain.
  // QUIC cannot follow HTTPS proxy server.
  EXPECT_CHECK_DEATH(ProxyChain::ForIpProtection({https1, quic2}).IsValid());
  EXPECT_CHECK_DEATH(
      ProxyChain::ForIpProtection({https1, https2, quic1, quic2}).IsValid());
  // Socks proxy server is not valid for multi-proxy chain.
  EXPECT_CHECK_DEATH(ProxyChain::ForIpProtection({socks, https1}).IsValid());
  EXPECT_CHECK_DEATH(
      ProxyChain::ForIpProtection({socks, https1, https2}).IsValid());
  EXPECT_CHECK_DEATH(ProxyChain::ForIpProtection({https1, socks}).IsValid());
  EXPECT_CHECK_DEATH(
      ProxyChain::ForIpProtection({https1, https2, socks}).IsValid());

#if !BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
  bool multi_proxy_chain_supported = false;
#else  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
  bool multi_proxy_chain_supported = true;
#endif
  // Multi-proxy chains are only supported in debug mode.
  EXPECT_EQ(ProxyChain({https1, https2}).IsValid(),
            multi_proxy_chain_supported);

#if !BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
  bool is_quic_supported = false;
#else  // BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
  bool is_quic_supported = true;
#endif
  // Multi-proxy chains are only supported in debug mode.
  EXPECT_EQ(ProxyChain({quic1}).IsValid(), is_quic_supported);

// If quic proxy support is enabled AND multi-proxy chain support is
// enabled, the following chains are valid. Otherwise, they are invalid.
#if !BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS) || \
    !BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
  bool is_multi_proxy_quic_supported = false;
#else
  bool is_multi_proxy_quic_supported = true;
#endif
  EXPECT_EQ(ProxyChain({quic1, https1}).IsValid(),
            is_multi_proxy_quic_supported);
  EXPECT_EQ(ProxyChain({quic1, quic2, https1, https2}).IsValid(),
            is_multi_proxy_quic_supported);
}

TEST(ProxyChainTest, Unequal) {
  // Ordered proxy chains.
  std::vector<ProxyChain> proxy_chain_list = {
      ProxyChain::Direct(),
      ProxyUriToProxyChain("foo:333", ProxyServer::SCHEME_HTTP),
      ProxyUriToProxyChain("foo:444", ProxyServer::SCHEME_HTTP),
      ProxyChain({ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS),
                  ProxyUriToProxyServer("foo:666", ProxyServer::SCHEME_HTTPS)}),
      ProxyUriToProxyChain("socks4://foo:33", ProxyServer::SCHEME_SOCKS4),
      ProxyUriToProxyChain("http://foo:33", ProxyServer::SCHEME_HTTP),
      ProxyChain({ProxyUriToProxyChain("bar:33", ProxyServer::SCHEME_HTTP)}),
      ProxyChain::ForIpProtection(
          {ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS),
           ProxyUriToProxyServer("foo:666", ProxyServer::SCHEME_HTTPS)})};

  // Unordered proxy chains.
  std::set<ProxyChain> proxy_chain_set(proxy_chain_list.begin(),
                                       proxy_chain_list.end());

  // Initial proxy chain list and set are equal.
  ASSERT_EQ(proxy_chain_list.size(), proxy_chain_set.size());

  for (const ProxyChain& proxy_chain1 : proxy_chain_list) {
    auto proxy_chain2 = proxy_chain_set.begin();
    // Chain set entries less than `proxy_chain1`.
    while (*proxy_chain2 < proxy_chain1) {
      EXPECT_TRUE(*proxy_chain2 < proxy_chain1);
      EXPECT_FALSE(proxy_chain1 < *proxy_chain2);
      EXPECT_FALSE(*proxy_chain2 == proxy_chain1);
      EXPECT_FALSE(proxy_chain1 == *proxy_chain2);
      ++proxy_chain2;
    }

    // Chain set entry for `proxy_chain1`.
    EXPECT_FALSE(*proxy_chain2 < proxy_chain1);
    EXPECT_FALSE(proxy_chain1 < *proxy_chain2);
    EXPECT_TRUE(*proxy_chain2 == proxy_chain1);
    EXPECT_TRUE(proxy_chain1 == *proxy_chain2);
    ++proxy_chain2;

    // Chain set entries greater than `proxy_chain1`.
    while (proxy_chain2 != proxy_chain_set.end() &&
           proxy_chain1 < *proxy_chain2) {
      EXPECT_FALSE(*proxy_chain2 < proxy_chain1);
      EXPECT_TRUE(proxy_chain1 < *proxy_chain2);
      EXPECT_FALSE(*proxy_chain2 == proxy_chain1);
      EXPECT_FALSE(proxy_chain1 == *proxy_chain2);
      ++proxy_chain2;
    }
    ASSERT_EQ(proxy_chain2, proxy_chain_set.end());
  }
}

TEST(ProxyChainTest, Equal) {
  ProxyServer proxy_server =
      ProxyUriToProxyServer("foo:11", ProxyServer::SCHEME_HTTP);

  ProxyChain proxy_chain1 = ProxyChain(proxy_server);
  ProxyChain proxy_chain2 = ProxyChain(std::vector<ProxyServer>{proxy_server});
  ProxyChain proxy_chain3 =
      ProxyChain(ProxyServer::SCHEME_HTTP, HostPortPair("foo", 11));

  EXPECT_FALSE(proxy_chain1 < proxy_chain2);
  EXPECT_FALSE(proxy_chain2 < proxy_chain1);
  EXPECT_TRUE(proxy_chain2 == proxy_chain1);
  EXPECT_TRUE(proxy_chain2 == proxy_chain1);

  EXPECT_FALSE(proxy_chain2 < proxy_chain3);
  EXPECT_FALSE(proxy_chain3 < proxy_chain2);
  EXPECT_TRUE(proxy_chain3 == proxy_chain2);
  EXPECT_TRUE(proxy_chain3 == proxy_chain2);
}

TEST(ProxyChainTest, PickleDirect) {
  ProxyChain proxy_chain = ProxyChain::Direct();
  base::Pickle pickle;
  proxy_chain.Persist(&pickle);
  base::PickleIterator iter(pickle);
  ProxyChain proxy_chain_from_pickle;
  proxy_chain_from_pickle.InitFromPickle(&iter);
  EXPECT_EQ(proxy_chain, proxy_chain_from_pickle);
}

TEST(ProxyChainTest, PickleOneProxy) {
  ProxyChain proxy_chain =
      ProxyChain(ProxyUriToProxyServer("foo:11", ProxyServer::SCHEME_HTTPS));
  base::Pickle pickle;
  proxy_chain.Persist(&pickle);
  base::PickleIterator iter(pickle);
  ProxyChain proxy_chain_from_pickle;
  proxy_chain_from_pickle.InitFromPickle(&iter);
  EXPECT_EQ(proxy_chain, proxy_chain_from_pickle);
}

#if !BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
// Multi-proxy chains that are not for Ip Protection are not allowed in release
// builds. If created, it should be considered invalid.
TEST(ProxyChainTest, MultiProxyChainNotForIpProtectionInvalidProxyChain) {
  ProxyChain invalid_chain =
      ProxyChain({ProxyUriToProxyServer("foo:11", ProxyServer::SCHEME_HTTPS),
                  ProxyUriToProxyServer("hoo:11", ProxyServer::SCHEME_HTTPS)});

  EXPECT_FALSE(invalid_chain.IsValid());
}
#else  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
TEST(ProxyChainTest, MultiProxyChain) {
  auto proxy_server1 =
      ProxyUriToProxyServer("foo:333", ProxyServer::SCHEME_HTTPS);
  auto proxy_server2 =
      ProxyUriToProxyServer("foo:444", ProxyServer::SCHEME_HTTPS);
  auto proxy_server3 =
      ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS);

  std::vector<ProxyServer> proxy_servers = {proxy_server1, proxy_server2,
                                            proxy_server3};
  auto proxy = ProxyChain(proxy_servers);

  EXPECT_FALSE(proxy.is_direct());
  EXPECT_FALSE(proxy.is_single_proxy());
  EXPECT_TRUE(proxy.is_multi_proxy());
  ASSERT_EQ(proxy.proxy_servers(), proxy_servers);
  ASSERT_EQ(proxy.length(), 3u);
  ASSERT_EQ(proxy.GetProxyServer(0), proxy_server1);
  ASSERT_EQ(proxy.GetProxyServer(1), proxy_server2);
  ASSERT_EQ(proxy.GetProxyServer(2), proxy_server3);

  // Ensure that proxy chains are equal even if one is for IP Protection.
  auto regular_proxy_chain = ProxyChain({proxy_server1, proxy_server2});
  auto ip_protection_proxy_chain =
      ProxyChain::ForIpProtection({proxy_server1, proxy_server2});
  EXPECT_TRUE(ip_protection_proxy_chain.is_for_ip_protection());
  EXPECT_EQ(regular_proxy_chain.proxy_servers(),
            ip_protection_proxy_chain.proxy_servers());
}

TEST(ProxyChainTest, MultiProxyChainsCanBeConvertedToForIpProtection) {
  ProxyChain proxy_chain =
      ProxyChain({ProxyUriToProxyServer("foo:555", ProxyServer::SCHEME_HTTPS),
                  ProxyUriToProxyServer("foo:666", ProxyServer::SCHEME_HTTPS)});
  ProxyChain copied_proxy_chain = proxy_chain;

  // Assert the proxy chain is currently not for ip protection.
  EXPECT_FALSE(proxy_chain.is_for_ip_protection());
  EXPECT_EQ(proxy_chain.ip_protection_chain_id(),
            ProxyChain::kNotIpProtectionChainId);

  // Convert proxy_chain to be for IP protection.
  proxy_chain =
      ProxyChain::ForIpProtection(std::move(proxy_chain.proxy_servers()));

  // Assert proxy_chain now shows it is for IP protection while copied proxy
  // chain still isn't.
  EXPECT_TRUE(proxy_chain.is_for_ip_protection());
  EXPECT_EQ(proxy_chain.ip_protection_chain_id(),
            ProxyChain::kDefaultIpProtectionChainId);
  EXPECT_FALSE(copied_proxy_chain.is_for_ip_protection());
  EXPECT_EQ(copied_proxy_chain.ip_protection_chain_id(),
            ProxyChain::kNotIpProtectionChainId);

  // Ensure servers contained are still equal.
  EXPECT_EQ(proxy_chain.proxy_servers(), copied_proxy_chain.proxy_servers());
}

TEST(ProxyChainTest, PickleTwoProxies) {
  ProxyChain proxy_chain =
      ProxyChain({ProxyUriToProxyServer("foo:11", ProxyServer::SCHEME_HTTPS),
                  ProxyUriToProxyServer("foo:22", ProxyServer::SCHEME_HTTPS)});
  base::Pickle pickle;
  proxy_chain.Persist(&pickle);
  base::PickleIterator iter(pickle);
  ProxyChain proxy_chain_from_pickle;
  proxy_chain_from_pickle.InitFromPickle(&iter);
  EXPECT_EQ(proxy_chain, proxy_chain_from_pickle);
}
#endif

}  // namespace

}  // namespace net
