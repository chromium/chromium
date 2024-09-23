// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_string_util.h"

#include <string>
#include <vector>

#include "build/buildflag.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/net_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// Test the creation of ProxyServer using ProxyUriToProxyServer, which parses
// inputs of the form [<scheme>"://"]<host>[":"<port>]. Verify that each part
// was labelled correctly, and the accessors all give the right data.
TEST(ProxySpecificationUtilTest, ProxyUriToProxyServer) {
  const struct {
    const char* const input_uri;
    const char* const expected_uri;
    ProxyServer::Scheme expected_scheme;
    const char* const expected_host;
    int expected_port;
    const char* const expected_pac_string;
  } tests[] = {
      // HTTP proxy URIs:
      {"foopy:10",  // No scheme.
       "foopy:10", ProxyServer::SCHEME_HTTP, "foopy", 10, "PROXY foopy:10"},
      {"http://foopy",  // No port.
       "foopy:80", ProxyServer::SCHEME_HTTP, "foopy", 80, "PROXY foopy:80"},
      {"http://foopy:10", "foopy:10", ProxyServer::SCHEME_HTTP, "foopy", 10,
       "PROXY foopy:10"},

      // IPv6 HTTP proxy URIs:
      {"[fedc:ba98:7654:3210:fedc:ba98:7654:3210]:10",  // No scheme.
       "[fedc:ba98:7654:3210:fedc:ba98:7654:3210]:10", ProxyServer::SCHEME_HTTP,
       "fedc:ba98:7654:3210:fedc:ba98:7654:3210", 10,
       "PROXY [fedc:ba98:7654:3210:fedc:ba98:7654:3210]:10"},
      {"http://[3ffe:2a00:100:7031::1]",  // No port.
       "[3ffe:2a00:100:7031::1]:80", ProxyServer::SCHEME_HTTP,
       "3ffe:2a00:100:7031::1", 80, "PROXY [3ffe:2a00:100:7031::1]:80"},

      // SOCKS4 proxy URIs:
      {"socks4://foopy",  // No port.
       "socks4://foopy:1080", ProxyServer::SCHEME_SOCKS4, "foopy", 1080,
       "SOCKS foopy:1080"},
      {"socks4://foopy:10", "socks4://foopy:10", ProxyServer::SCHEME_SOCKS4,
       "foopy", 10, "SOCKS foopy:10"},

      // SOCKS5 proxy URIs:
      {"socks5://foopy",  // No port.
       "socks5://foopy:1080", ProxyServer::SCHEME_SOCKS5, "foopy", 1080,
       "SOCKS5 foopy:1080"},
      {"socks5://foopy:10", "socks5://foopy:10", ProxyServer::SCHEME_SOCKS5,
       "foopy", 10, "SOCKS5 foopy:10"},

      // SOCKS proxy URIs (should default to SOCKS5)
      {"socks://foopy",  // No port.
       "socks5://foopy:1080", ProxyServer::SCHEME_SOCKS5, "foopy", 1080,
       "SOCKS5 foopy:1080"},
      {"socks://foopy:10", "socks5://foopy:10", ProxyServer::SCHEME_SOCKS5,
       "foopy", 10, "SOCKS5 foopy:10"},

      // HTTPS proxy URIs:
      {"https://foopy",  // No port
       "https://foopy:443", ProxyServer::SCHEME_HTTPS, "foopy", 443,
       "HTTPS foopy:443"},
      {"https://foopy:10",  // Non-standard port
       "https://foopy:10", ProxyServer::SCHEME_HTTPS, "foopy", 10,
       "HTTPS foopy:10"},
      {"https://1.2.3.4:10",  // IP Address
       "https://1.2.3.4:10", ProxyServer::SCHEME_HTTPS, "1.2.3.4", 10,
       "HTTPS 1.2.3.4:10"},

#if BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
      // QUIC proxy URIs:
      {"quic://foopy",  // no port
       "quic://foopy:443", ProxyServer::SCHEME_QUIC, "foopy", 443,
       "QUIC foopy:443"},
      {"quic://foopy:80", "quic://foopy:80", ProxyServer::SCHEME_QUIC, "foopy",
       80, "QUIC foopy:80"},
#endif  // BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)

      // Hostname canonicalization:
      {"[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:10",  // No scheme.
       "[fedc:ba98:7654:3210:fedc:ba98:7654:3210]:10", ProxyServer::SCHEME_HTTP,
       "fedc:ba98:7654:3210:fedc:ba98:7654:3210", 10,
       "PROXY [fedc:ba98:7654:3210:fedc:ba98:7654:3210]:10"},
      {"http://[::192.9.5.5]", "[::c009:505]:80", ProxyServer::SCHEME_HTTP,
       "::c009:505", 80, "PROXY [::c009:505]:80"},
      {"http://[::FFFF:129.144.52.38]:80", "[::ffff:8190:3426]:80",
       ProxyServer::SCHEME_HTTP, "::ffff:8190:3426", 80,
       "PROXY [::ffff:8190:3426]:80"},
      {"http://f\u00fcpy:85", "xn--fpy-hoa:85", ProxyServer::SCHEME_HTTP,
       "xn--fpy-hoa", 85, "PROXY xn--fpy-hoa:85"},
      {"https://0xA.020.3.4:443", "https://10.16.3.4:443",
       ProxyServer::SCHEME_HTTPS, "10.16.3.4", 443, "HTTPS 10.16.3.4:443"},
      {"http://FoO.tEsT:80", "foo.test:80", ProxyServer::SCHEME_HTTP,
       "foo.test", 80, "PROXY foo.test:80"},
  };

  for (const auto& test : tests) {
    ProxyServer uri = ProxyUriToProxyServer(
        test.input_uri, ProxyServer::SCHEME_HTTP, /*is_quic_allowed=*/true);
    EXPECT_TRUE(uri.is_valid());
    EXPECT_EQ(test.expected_uri, ProxyServerToProxyUri(uri));
    EXPECT_EQ(test.expected_scheme, uri.scheme());
    EXPECT_EQ(test.expected_host, uri.host_port_pair().host());
    EXPECT_EQ(test.expected_port, uri.host_port_pair().port());
    EXPECT_EQ(test.expected_pac_string, ProxyServerToPacResultElement(uri));
  }
}

#if BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
// In a build where the quic proxy support build flag is enabled, if the
// boolean for allowing quic proxy support is false, it will be considered in an
// invalid scheme as QUIC should not be parsed.
TEST(ProxySpecificationUtilTest,
     ProxyUriToProxyServerBuildFlagEnabledQuicDisallowedIsInvalid) {
  ProxyServer proxy_server = ProxyUriToProxyServer(
      "quic://foopy:443", ProxyServer::SCHEME_HTTP, /*is_quic_allowed=*/false);
  EXPECT_FALSE(proxy_server.is_valid());
  EXPECT_EQ(ProxyServer::SCHEME_INVALID, proxy_server.scheme());
}
#else
// In a build where the quic proxy support build flag is disabled, if the
// boolean for allowing quic proxy support is true, it will be considered in an
// invalid scheme as QUIC is not allowed in this type of build.
TEST(ProxySpecificationUtilTest,
     ProxyUriToProxyServerBuildFlagDisabledQuicAllowedIsInvalid) {
  ProxyServer proxy_server = ProxyUriToProxyServer(
      "quic://foopy:443", ProxyServer::SCHEME_HTTP, /*is_quic_allowed=*/true);
  EXPECT_FALSE(proxy_server.is_valid());
  EXPECT_EQ(ProxyServer::SCHEME_INVALID, proxy_server.scheme());
}
#endif  // BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)

// Test parsing of the special URI form "direct://".
TEST(ProxySpecificationUtilTest, DirectProxyUriToProxyChain) {
  const char* const uris[] = {
      "direct://",
      "DIRECT://",
      "DiReCt://",
  };

  for (const char* uri : uris) {
    ProxyChain valid_uri = ProxyUriToProxyChain(uri, ProxyServer::SCHEME_HTTP);
    EXPECT_TRUE(valid_uri.IsValid());
    EXPECT_TRUE(valid_uri.is_direct());
  }

  // Direct is not allowed a host/port.
  ProxyChain invalid_uri =
      ProxyUriToProxyChain("direct://xyz", ProxyServer::SCHEME_HTTP);
  EXPECT_FALSE(invalid_uri.IsValid());
  EXPECT_FALSE(invalid_uri.is_direct());
}

// A multi-proxy string containing URIs is not acceptable input for the
// ProxyUriToProxyChain function and should return an invalid `ProxyChain()`.
TEST(ProxySpecificationUtilTest, ProxyUriToProxyChainWithBracketsInvalid) {
  // Release builds should return an invalid proxy chain for multi-proxy chains.
  const char* const invalid_multi_proxy_uris[] = {
      "[]",
      "[direct://]",
      "[https://foopy]",
      "[https://foopy https://hoopy]",
  };

  for (const char* uri : invalid_multi_proxy_uris) {
    ProxyChain multi_proxy_uri =
        ProxyUriToProxyChain(uri, ProxyServer::SCHEME_HTTP);
    EXPECT_FALSE(multi_proxy_uri.IsValid());
    EXPECT_FALSE(multi_proxy_uri.is_direct());
  }
}

// Test parsing some invalid inputs.
TEST(ProxySpecificationUtilTest, InvalidProxyUriToProxyServer) {
  const char* const tests[] = {
      "",
      "   ",
      "dddf:",         // not a valid port
      "dddd:d",        // not a valid port
      "http://",       // not a valid host/port.
      "direct://",     // direct is not a valid proxy server.
      "http:/",        // ambiguous, but will fail because of bad port.
      "http:",         // ambiguous, but will fail because of bad port.
      "foopy.111",     // Interpreted as invalid IPv4 address.
      "foo.test/"      // Paths disallowed.
      "foo.test:123/"  // Paths disallowed.
      "foo.test/foo"   // Paths disallowed.
  };

  for (const char* test : tests) {
    SCOPED_TRACE(test);
    ProxyServer uri = ProxyUriToProxyServer(test, ProxyServer::SCHEME_HTTP);
    EXPECT_FALSE(uri.is_valid());
    EXPECT_FALSE(uri.is_http());
    EXPECT_FALSE(uri.is_socks());
  }
}

// Test that LWS (SP | HT) is disregarded from the ends.
TEST(ProxySpecificationUtilTest, WhitespaceProxyUriToProxyServer) {
  const char* const tests[] = {
      "  foopy:80",
      "foopy:80   \t",
      "  \tfoopy:80  ",
  };

  for (const char* test : tests) {
    ProxyServer uri = ProxyUriToProxyServer(test, ProxyServer::SCHEME_HTTP);
    EXPECT_EQ("foopy:80", ProxyServerToProxyUri(uri));
  }
}

// Test parsing a ProxyServer from a PAC representation.
TEST(ProxySpecificationUtilTest, PacResultElementToProxyServer) {
  const struct {
    const char* const input_pac;
    const char* const expected_uri;
  } tests[] = {
      {
          "PROXY foopy:10",
          "foopy:10",
      },
      {
          "   PROXY    foopy:10   ",
          "foopy:10",
      },
      {
          "pRoXy foopy:10",
          "foopy:10",
      },
      {
          "PROXY foopy",  // No port.
          "foopy:80",
      },
      {
          "socks foopy",
          "socks4://foopy:1080",
      },
      {
          "socks4 foopy",
          "socks4://foopy:1080",
      },
      {
          "socks5 foopy",
          "socks5://foopy:1080",
      },
      {
          "socks5 foopy:11",
          "socks5://foopy:11",
      },
      {
          "https foopy",
          "https://foopy:443",
      },
      {
          "https foopy:10",
          "https://foopy:10",
      },
      {"PROXY [FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:10",
       "[fedc:ba98:7654:3210:fedc:ba98:7654:3210]:10"},
      {"PROXY f\u00fcpy:85", "xn--fpy-hoa:85"},
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(test.input_pac);
    ProxyServer server = PacResultElementToProxyServer(test.input_pac);
    EXPECT_TRUE(server.is_valid());
    EXPECT_EQ(test.expected_uri, ProxyServerToProxyUri(server));

    ProxyChain chain = PacResultElementToProxyChain(test.input_pac);
    EXPECT_TRUE(chain.IsValid());
    if (!chain.is_direct()) {
      EXPECT_EQ(test.expected_uri, ProxyServerToProxyUri(chain.First()));
    }
  }
}

// Test parsing a ProxyServer from an invalid PAC representation.
TEST(ProxySpecificationUtilTest, InvalidPacResultElementToProxyServer) {
  const char* const tests[] = {
      "PROXY",                   // missing host/port.
      "HTTPS",                   // missing host/port.
      "SOCKS",                   // missing host/port.
      "DIRECT foopy:10",         // direct cannot have host/port.
      "INVALIDSCHEME",           // unrecognized scheme.
      "INVALIDSCHEME foopy:10",  // unrecognized scheme.
      "HTTP foopy:10",           // http scheme should be "PROXY"
  };

  for (const char* test : tests) {
    SCOPED_TRACE(test);
    ProxyServer server = PacResultElementToProxyServer(test);
    EXPECT_FALSE(server.is_valid());

    ProxyChain chain = PacResultElementToProxyChain(test);
    EXPECT_FALSE(chain.IsValid());
  }
}

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
// A multi-proxy chain that contains any mention of direct will be considered an
// invalid `ProxyChain()`.
TEST(ProxySpecificationUtilTest,
     MultiProxyUrisToProxyChainMultiProxyDirectIsInvalid) {
  const char* const invalid_multi_proxy_uris[] = {
      "[direct://xyz]",             // direct with ports
      "[direct:// direct://]",      // Two directs in chain
      "[direct:// https://foopy]",  // direct first in chain
      "[https://foopy direct://]",  // direct later in chain
  };

  for (const char* uri : invalid_multi_proxy_uris) {
    ProxyChain multi_proxy_uri =
        MultiProxyUrisToProxyChain(uri, ProxyServer::SCHEME_HTTPS);
    EXPECT_FALSE(multi_proxy_uri.IsValid());
    EXPECT_FALSE(multi_proxy_uri.is_direct());
  }
}

// A input containing a single uri of direct will be valid.
TEST(ProxySpecificationUtilTest,
     MultiProxyUrisToProxyChainSingleDirectIsValid) {
  const char* const valid_direct_uris[] = {
      "direct://",    // non-bracketed direct
      "[direct://]",  // bracketed direct
  };

  for (const char* uri : valid_direct_uris) {
    ProxyChain multi_proxy_uri =
        MultiProxyUrisToProxyChain(uri, ProxyServer::SCHEME_HTTPS);
    EXPECT_TRUE(multi_proxy_uri.IsValid());
    EXPECT_TRUE(multi_proxy_uri.is_direct());
  }
}

TEST(ProxySpecificationUtilTest, MultiProxyUrisToProxyChainValid) {
  const struct {
    const char* const input_uri;
    const std::vector<std::string> expected_uris;
    ProxyServer::Scheme expected_scheme;
  } tests[] = {
      // 1 Proxy (w/ and w/o brackets):
      {"[https://foopy:443]", {"https://foopy:443"}, ProxyServer::SCHEME_HTTPS},
      {"https://foopy:443", {"https://foopy:443"}, ProxyServer::SCHEME_HTTPS},

      // 2 Proxies:
      {"[https://foopy:443 https://hoopy:443]",
       {"https://foopy:443", "https://hoopy:443"},
       ProxyServer::SCHEME_HTTPS},

      // Extra padding in uris string ignored:
      {" [https://foopy:443 https://hoopy:443] ",
       {"https://foopy:443", "https://hoopy:443"},
       ProxyServer::SCHEME_HTTPS},
      {"[\thttps://foopy:443 https://hoopy:443\t       ] ",
       {"https://foopy:443", "https://hoopy:443"},
       ProxyServer::SCHEME_HTTPS},
      {"     \t[       https://foopy:443 https://hoopy:443\t        ]",
       {"https://foopy:443", "https://hoopy:443"},
       ProxyServer::SCHEME_HTTPS},
      {"[https://foopy:443  https://hoopy:443]",
       {"https://foopy:443", "https://hoopy:443"},
       ProxyServer::SCHEME_HTTPS},  // Delimiter is two spaces.
      {"[https://foopy \thttps://hoopy]",
       {"https://foopy:443", "https://hoopy:443"},
       ProxyServer::SCHEME_HTTPS},  // Delimiter is followed by tab.

      // 3 Proxies:
      {"[https://foopy:443 https://hoopy:443 https://loopy:443]",
       {"https://foopy:443", "https://hoopy:443", "https://loopy:443"},
       ProxyServer::SCHEME_HTTPS},
  };

  for (const auto& test : tests) {
    ProxyChain proxy_chain =
        MultiProxyUrisToProxyChain(test.input_uri, test.expected_scheme);

    EXPECT_TRUE(proxy_chain.IsValid());
    EXPECT_EQ(proxy_chain.length(), test.expected_uris.size());

    std::vector<ProxyServer> proxies = proxy_chain.proxy_servers();
    for (size_t i = 0; i < proxies.size(); i++) {
      const ProxyServer& proxy = proxies[i];
      EXPECT_TRUE(proxy.is_valid());
      EXPECT_EQ(test.expected_uris[i], ProxyServerToProxyUri(proxy));
    }
  }
}

#if BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
// Quic proxy schemes are parsed properly
TEST(ProxySpecificationUtilTest, MultiProxyUrisToProxyChainValidQuic) {
  const struct {
    const char* const input_uri;
    const std::vector<std::string> expected_uris;
    ProxyServer::Scheme default_scheme;
    const std::vector<ProxyServer::Scheme> expected_schemes;
  } tests[] = {
      // single quic proxy scheme (unbracketed)
      {"quic://foopy",  // missing port number
       {"quic://foopy:443"},
       ProxyServer::SCHEME_HTTP,
       {ProxyServer::SCHEME_QUIC}},
      {"quic://foopy:80",
       {"quic://foopy:80"},
       ProxyServer::SCHEME_HTTP,
       {ProxyServer::SCHEME_QUIC}},

      // single quic proxy scheme (bracketed)
      {"[quic://foopy:80]",
       {"quic://foopy:80"},
       ProxyServer::SCHEME_HTTP,
       {ProxyServer::SCHEME_QUIC}},

      // multi-proxy chain
      // 2 quic schemes in a row
      {"[quic://foopy:80 quic://loopy:80]",
       {"quic://foopy:80", "quic://loopy:80"},
       ProxyServer::SCHEME_HTTP,
       {ProxyServer::SCHEME_QUIC, ProxyServer::SCHEME_QUIC}},
      // Quic scheme followed by HTTPS in a row
      {"[quic://foopy:80 https://loopy:80]",
       {"quic://foopy:80", "https://loopy:80"},
       ProxyServer::SCHEME_HTTP,
       {ProxyServer::SCHEME_QUIC, ProxyServer::SCHEME_HTTPS}},
  };

  for (const auto& test : tests) {
    ProxyChain proxy_chain = MultiProxyUrisToProxyChain(
        test.input_uri, test.default_scheme, /*is_quic_allowed=*/true);

    EXPECT_TRUE(proxy_chain.IsValid());
    EXPECT_EQ(proxy_chain.length(), test.expected_uris.size());

    std::vector<ProxyServer> proxies = proxy_chain.proxy_servers();
    for (size_t i = 0; i < proxies.size(); i++) {
      const ProxyServer& proxy = proxies[i];
      EXPECT_TRUE(proxy.is_valid());
      EXPECT_EQ(test.expected_uris[i], ProxyServerToProxyUri(proxy));
      EXPECT_EQ(test.expected_schemes[i], proxy.scheme());
    }
  }
}

// If a multi-proxy chain contains a quic scheme proxy, it must only be followed
// by another quic or https proxy. This ensures this logic still applies.
TEST(ProxySpecificationUtilTest, MultiProxyUrisToProxyChainInvalidQuicCombo) {
  ProxyChain proxy_chain = MultiProxyUrisToProxyChain(
      "[https://loopy:80 quic://foopy:80]", ProxyServer::SCHEME_HTTP);

  EXPECT_FALSE(proxy_chain.IsValid());
}

#endif  // BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)

// If the input URIs is invalid, an invalid `ProxyChain()` will be returned.
TEST(ProxySpecificationUtilTest,
     MultiProxyUrisToProxyChainInvalidFormatReturnsInvalidProxyChain) {
  const char* const invalid_multi_proxy_uris[] = {
      "",                                 // Empty string
      "   ",                              // String with only spaces
      "[]",                               // No proxies within brackets
      "https://foopy https://hoopy",      // Missing brackets
      "[https://foopy https://hoopy",     // Missing bracket
      "https://foopy https://hoopy]",     // Missing bracket
      "https://foopy \t   https://hoopy"  // Missing brackets and bad delimiter
  };

  for (const char* uri : invalid_multi_proxy_uris) {
    ProxyChain multi_proxy_uri =
        MultiProxyUrisToProxyChain(uri, ProxyServer::SCHEME_HTTPS);
    EXPECT_FALSE(multi_proxy_uri.IsValid());
    EXPECT_FALSE(multi_proxy_uri.is_direct());
  }
}
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
}  // namespace
}  // namespace net
