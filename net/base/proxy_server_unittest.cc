// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_server.h"
#include "base/cxx17_backports.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Test the creation of ProxyServer using ProxyServer::FromURI, which parses
// inputs of the form [<scheme>"://"]<host>[":"<port>]. Verify that each part
// was labelled correctly, and the accessors all give the right data.
TEST(ProxyServerTest, FromURI) {
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
       "foopy:10",
       ProxyServer::SCHEME_HTTP,
       "foopy",
       10,
       "PROXY foopy:10"},
      {"http://foopy",  // No port.
       "foopy:80",
       ProxyServer::SCHEME_HTTP,
       "foopy",
       80,
       "PROXY foopy:80"},
      {"http://foopy:10",
       "foopy:10",
       ProxyServer::SCHEME_HTTP,
       "foopy",
       10,
       "PROXY foopy:10"},

      // IPv6 HTTP proxy URIs:
      {"[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:10",  // No scheme.
       "[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:10",
       ProxyServer::SCHEME_HTTP,
       "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210",
       10,
       "PROXY [FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:10"},
      {"http://[3ffe:2a00:100:7031::1]",  // No port.
       "[3ffe:2a00:100:7031::1]:80",
       ProxyServer::SCHEME_HTTP,
       "3ffe:2a00:100:7031::1",
       80,
       "PROXY [3ffe:2a00:100:7031::1]:80"},
      {"http://[::192.9.5.5]",
       "[::192.9.5.5]:80",
       ProxyServer::SCHEME_HTTP,
       "::192.9.5.5",
       80,
       "PROXY [::192.9.5.5]:80"},
      {"http://[::FFFF:129.144.52.38]:80",
       "[::FFFF:129.144.52.38]:80",
       ProxyServer::SCHEME_HTTP,
       "::FFFF:129.144.52.38",
       80,
       "PROXY [::FFFF:129.144.52.38]:80"},

      // SOCKS4 proxy URIs:
      {"socks4://foopy",  // No port.
       "socks4://foopy:1080",
       ProxyServer::SCHEME_SOCKS4,
       "foopy",
       1080,
       "SOCKS foopy:1080"},
      {"socks4://foopy:10",
       "socks4://foopy:10",
       ProxyServer::SCHEME_SOCKS4,
       "foopy",
       10,
       "SOCKS foopy:10"},

      // SOCKS5 proxy URIs
      {"socks5://foopy",  // No port.
       "socks5://foopy:1080",
       ProxyServer::SCHEME_SOCKS5,
       "foopy",
       1080,
       "SOCKS5 foopy:1080"},
      {"socks5://foopy:10",
       "socks5://foopy:10",
       ProxyServer::SCHEME_SOCKS5,
       "foopy",
       10,
       "SOCKS5 foopy:10"},

      // SOCKS proxy URIs (should default to SOCKS5)
      {"socks://foopy",  // No port.
       "socks5://foopy:1080",
       ProxyServer::SCHEME_SOCKS5,
       "foopy",
       1080,
       "SOCKS5 foopy:1080"},
      {"socks://foopy:10",
       "socks5://foopy:10",
       ProxyServer::SCHEME_SOCKS5,
       "foopy",
       10,
       "SOCKS5 foopy:10"},

      // HTTPS proxy URIs:
      {"https://foopy",  // No port
       "https://foopy:443",
       ProxyServer::SCHEME_HTTPS,
       "foopy",
       443,
       "HTTPS foopy:443"},
      {"https://foopy:10",  // Non-standard port
       "https://foopy:10",
       ProxyServer::SCHEME_HTTPS,
       "foopy",
       10,
       "HTTPS foopy:10"},
      {"https://1.2.3.4:10",  // IP Address
       "https://1.2.3.4:10",
       ProxyServer::SCHEME_HTTPS,
       "1.2.3.4",
       10,
       "HTTPS 1.2.3.4:10"},
  };

  for (size_t i = 0; i < base::size(tests); ++i) {
    ProxyServer uri =
        ProxyServer::FromURI(tests[i].input_uri, ProxyServer::SCHEME_HTTP);
    EXPECT_TRUE(uri.is_valid());
    EXPECT_FALSE(uri.is_direct());
    EXPECT_EQ(tests[i].expected_uri, uri.ToURI());
    EXPECT_EQ(tests[i].expected_scheme, uri.scheme());
    EXPECT_EQ(tests[i].expected_host, uri.host_port_pair().host());
    EXPECT_EQ(tests[i].expected_port, uri.host_port_pair().port());
    EXPECT_EQ(tests[i].expected_pac_string, uri.ToPacString());
  }
}

TEST(ProxyServerTest, DefaultConstructor) {
  ProxyServer proxy_server;
  EXPECT_FALSE(proxy_server.is_valid());
}

// Test parsing of the special URI form "direct://". Analagous to the "DIRECT"
// entry in a PAC result.
TEST(ProxyServerTest, Direct) {
  ProxyServer uri = ProxyServer::FromURI("direct://", ProxyServer::SCHEME_HTTP);
  EXPECT_TRUE(uri.is_valid());
  EXPECT_TRUE(uri.is_direct());
  EXPECT_EQ("direct://", uri.ToURI());
  EXPECT_EQ("DIRECT", uri.ToPacString());
}

// Test parsing some invalid inputs.
TEST(ProxyServerTest, Invalid) {
  const char* const tests[] = {
    "",
    "   ",
    "dddf:",   // not a valid port
    "dddd:d",  // not a valid port
    "http://",  // not a valid host/port.
    "direct://xyz",  // direct is not allowed a host/port.
    "http:/",  // ambiguous, but will fail because of bad port.
    "http:",  // ambiguous, but will fail because of bad port.
  };

  for (size_t i = 0; i < base::size(tests); ++i) {
    ProxyServer uri = ProxyServer::FromURI(tests[i], ProxyServer::SCHEME_HTTP);
    EXPECT_FALSE(uri.is_valid());
    EXPECT_FALSE(uri.is_direct());
    EXPECT_FALSE(uri.is_http());
    EXPECT_FALSE(uri.is_socks());
  }
}

// Test that LWS (SP | HT) is disregarded from the ends.
TEST(ProxyServerTest, Whitespace) {
  const char* const tests[] = {
    "  foopy:80",
    "foopy:80   \t",
    "  \tfoopy:80  ",
  };

  for (size_t i = 0; i < base::size(tests); ++i) {
    ProxyServer uri = ProxyServer::FromURI(tests[i], ProxyServer::SCHEME_HTTP);
    EXPECT_EQ("foopy:80", uri.ToURI());
  }
}

// Test parsing a ProxyServer from a PAC representation.
TEST(ProxyServerTest, FromPACString) {
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
  };

  for (size_t i = 0; i < base::size(tests); ++i) {
    ProxyServer uri = ProxyServer::FromPacString(tests[i].input_pac);
    EXPECT_TRUE(uri.is_valid());
    EXPECT_EQ(tests[i].expected_uri, uri.ToURI());
  }
}

// Test parsing a ProxyServer from an invalid PAC representation.
TEST(ProxyServerTest, FromPACStringInvalid) {
  const char* const tests[] = {
    "PROXY",  // missing host/port.
    "HTTPS",  // missing host/port.
    "SOCKS",  // missing host/port.
    "DIRECT foopy:10",  // direct cannot have host/port.
  };

  for (size_t i = 0; i < base::size(tests); ++i) {
    ProxyServer uri = ProxyServer::FromPacString(tests[i]);
    EXPECT_FALSE(uri.is_valid());
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
       ProxyServer::FromURI("foo:11", ProxyServer::SCHEME_HTTP),
       ProxyServer::FromURI("http://foo:11", ProxyServer::SCHEME_HTTP), 0},
      {// Port is different.
       ProxyServer::FromURI("foo:333", ProxyServer::SCHEME_HTTP),
       ProxyServer::FromURI("foo:444", ProxyServer::SCHEME_HTTP), -1},
      {// Host is different.
       ProxyServer::FromURI("foo:33", ProxyServer::SCHEME_HTTP),
       ProxyServer::FromURI("bar:33", ProxyServer::SCHEME_HTTP), 1},
      {// Scheme is different.
       ProxyServer::FromURI("socks4://foo:33", ProxyServer::SCHEME_HTTP),
       ProxyServer::FromURI("http://foo:33", ProxyServer::SCHEME_HTTP), 1},
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
    auto proxy = ProxyServer::FromPacString("PROXY foo");
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_TRUE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_TRUE(proxy.is_http_like());
    EXPECT_FALSE(proxy.is_secure_http_like());
  }

  // HTTPS proxy.
  {
    auto proxy = ProxyServer::FromPacString("HTTPS foo");
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_TRUE(proxy.is_https());
    EXPECT_TRUE(proxy.is_http_like());
    EXPECT_TRUE(proxy.is_secure_http_like());
  }

  // QUIC proxy.
  {
    auto proxy = ProxyServer::FromPacString("QUIC foo");
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_TRUE(proxy.is_http_like());
    EXPECT_TRUE(proxy.is_secure_http_like());
  }

  // SOCKS5 proxy.
  {
    auto proxy = ProxyServer::FromPacString("SOCKS5 foo");
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_FALSE(proxy.is_http_like());
    EXPECT_FALSE(proxy.is_secure_http_like());
  }

  // DIRECT
  {
    auto proxy = ProxyServer::FromPacString("DIRECT");
    ASSERT_TRUE(proxy.is_valid());
    EXPECT_FALSE(proxy.is_http());
    EXPECT_FALSE(proxy.is_https());
    EXPECT_FALSE(proxy.is_http_like());
    EXPECT_FALSE(proxy.is_secure_http_like());
  }
}

}  // namespace

}  // namespace net
