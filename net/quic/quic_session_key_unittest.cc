// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_key.h"

#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "net/base/session_usage.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/socket_tag.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

// Check for equality of session keys, and inequality when various pieces of the
// key differ. The SocketTag is only used on Android, and the NAK is only used
// when network partitioning is enabled.
TEST(QuicSessionKeyTest, Equality) {
  QuicSessionKey key(HostPortPair("www.example.org", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false);
  EXPECT_EQ(key,
            QuicSessionKey("www.example.org", 80, PRIVACY_MODE_DISABLED,
                           ProxyChain::Direct(), SessionUsage::kDestination,
                           SocketTag(), NetworkAnonymizationKey(),
                           SecureDnsPolicy::kAllow,
                           /*require_dns_https_alpn=*/false));
  EXPECT_EQ(key,
            QuicSessionKey(quic::QuicServerId("www.example.org", 80),
                           PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                           SessionUsage::kDestination, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                           /*require_dns_https_alpn=*/false));
  EXPECT_NE(
      key, QuicSessionKey(HostPortPair("otherproxy", 80), PRIVACY_MODE_DISABLED,
                          ProxyChain::Direct(), SessionUsage::kDestination,
                          SocketTag(), NetworkAnonymizationKey(),
                          SecureDnsPolicy::kAllow,
                          /*require_dns_https_alpn=*/false));
  EXPECT_NE(key,
            QuicSessionKey(HostPortPair("www.example.org", 80),
                           PRIVACY_MODE_ENABLED, ProxyChain::Direct(),
                           SessionUsage::kDestination, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                           /*require_dns_https_alpn=*/false));
  EXPECT_NE(key, QuicSessionKey(
                     HostPortPair("www.example.org", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain::FromSchemeHostAndPort(
                         ProxyServer::Scheme::SCHEME_HTTPS, "otherproxy", 443),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false));
  EXPECT_NE(key, QuicSessionKey(
                     HostPortPair("www.example.org", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kProxy, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_NE(key,
            QuicSessionKey(HostPortPair("www.example.org", 80),
                           PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                           SessionUsage::kDestination, SocketTag(999, 999),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                           /*require_dns_https_alpn=*/false));
#endif  // BUILDFLAG(IS_ANDROID)
  if (NetworkAnonymizationKey::IsPartitioningEnabled()) {
    EXPECT_NE(key, QuicSessionKey(HostPortPair("www.example.org", 80),
                                  PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                                  SessionUsage::kDestination, SocketTag(),
                                  NetworkAnonymizationKey::CreateSameSite(
                                      SchemefulSite(GURL("http://a.test/"))),
                                  SecureDnsPolicy::kAllow,
                                  /*require_dns_https_alpn=*/false));
  }
  EXPECT_NE(key,
            QuicSessionKey(HostPortPair("www.example.org", 80),
                           PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                           SessionUsage::kDestination, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                           /*require_dns_https_alpn=*/false));
  EXPECT_NE(key,
            QuicSessionKey("www.example.org", 80, PRIVACY_MODE_DISABLED,
                           ProxyChain::Direct(), SessionUsage::kDestination,
                           SocketTag(), NetworkAnonymizationKey(),
                           SecureDnsPolicy::kAllow,
                           /*require_dns_https_alpn=*/true));
}

// The operator< implementation is suitable for storing distinct keys in a set.
TEST(QuicSessionKeyTest, Set) {
  std::vector<QuicSessionKey> session_keys = {
      QuicSessionKey(HostPortPair("www.example.org", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false),
      QuicSessionKey(HostPortPair("otherproxy", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false),
      QuicSessionKey(HostPortPair("www.example.org", 80), PRIVACY_MODE_ENABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false),
      QuicSessionKey(HostPortPair("www.example.org", 80),
                     PRIVACY_MODE_ENABLED_WITHOUT_CLIENT_CERTS,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false),
      QuicSessionKey(HostPortPair("www.example.org", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain::FromSchemeHostAndPort(
                         ProxyServer::Scheme::SCHEME_HTTPS, "otherproxy", 443),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false),
      QuicSessionKey(HostPortPair("www.example.org", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain({
                         ProxyServer::FromSchemeHostAndPort(
                             ProxyServer::Scheme::SCHEME_HTTPS, "proxy1", 443),
                         ProxyServer::FromSchemeHostAndPort(
                             ProxyServer::Scheme::SCHEME_HTTPS, "proxy2", 443),
                     }),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false),
      QuicSessionKey(HostPortPair("www.example.org", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kProxy, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false),
#if BUILDFLAG(IS_ANDROID)
      QuicSessionKey(HostPortPair("www.example.org", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(999, 999), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false),
#endif  // BUILDFLAG(IS_ANDROID)
      QuicSessionKey(HostPortPair("www.example.org", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kDisable,
                     /*require_dns_https_alpn=*/false),
      QuicSessionKey(HostPortPair("www.example.org", 80), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/true),
  };
  if (NetworkAnonymizationKey::IsPartitioningEnabled()) {
    session_keys.emplace_back(HostPortPair("www.example.org", 80),
                              PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                              SessionUsage::kDestination, SocketTag(),
                              NetworkAnonymizationKey::CreateSameSite(
                                  SchemefulSite(GURL("http://a.test/"))),
                              SecureDnsPolicy::kAllow,
                              /*require_dns_https_alpn=*/false);
  }
  std::set<QuicSessionKey> key_set(session_keys.begin(), session_keys.end());
  ASSERT_EQ(session_keys.size(), key_set.size());
}

}  // namespace

}  // namespace net
