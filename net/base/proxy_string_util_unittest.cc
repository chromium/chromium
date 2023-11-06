// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_string_util.h"

#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
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
    ProxyServer uri =
        ProxyUriToProxyServer(test.input_uri, ProxyServer::SCHEME_HTTP);
    EXPECT_TRUE(uri.is_valid());
    EXPECT_FALSE(uri.is_direct());
    EXPECT_EQ(test.expected_uri, ProxyServerToProxyUri(uri));
    EXPECT_EQ(test.expected_scheme, uri.scheme());
    EXPECT_EQ(test.expected_host, uri.host_port_pair().host());
    EXPECT_EQ(test.expected_port, uri.host_port_pair().port());
    EXPECT_EQ(test.expected_pac_string, ProxyServerToPacResultElement(uri));
  }
}

// Test parsing of the special URI form "direct://". analogous to the "DIRECT"
// element in a PAC result.
TEST(ProxySpecificationUtilTest, DirectProxyUriToProxyServer) {
  ProxyServer uri =
      ProxyUriToProxyServer("direct://", ProxyServer::SCHEME_HTTP);
  EXPECT_TRUE(uri.is_valid());
  EXPECT_TRUE(uri.is_direct());
  EXPECT_EQ("direct://", ProxyServerToProxyUri(uri));
  EXPECT_EQ("DIRECT", ProxyServerToPacResultElement(uri));
}

// Test parsing some invalid inputs.
TEST(ProxySpecificationUtilTest, InvalidProxyUriToProxyServer) {
  const char* const tests[] = {
      "",
      "   ",
      "dddf:",         // not a valid port
      "dddd:d",        // not a valid port
      "http://",       // not a valid host/port.
      "direct://xyz",  // direct is not allowed a host/port.
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
    EXPECT_FALSE(uri.is_direct());
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
          " direct  ",
          "direct://",
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

    // TODO(https://crbug.com/1491092): Split this into a new test when
    // `PacResultElementToProxyChain()` does more than just wrap
    // `PacResultElementToProxyServer()`.
    ProxyChain chain = PacResultElementToProxyChain(test.input_pac);
    EXPECT_TRUE(chain.IsValid());
    EXPECT_EQ(test.expected_uri, ProxyServerToProxyUri(chain.proxy_server()));
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

    // TODO(https://crbug.com/1491092): Split this into a new test when
    // `PacResultElementToProxyChain()` does more than just wrap
    // `PacResultElementToProxyServer()`.
    ProxyChain chain = PacResultElementToProxyChain(test);
    EXPECT_FALSE(chain.IsValid());
  }
}

}  // namespace
}  // namespace net
