// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_key.h"

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/socket_tag.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

static const url::SchemeHostPort kHost("https", "www.example.com", 443);

}  // namespace

// These tests are similar to SpdySessionKeyTest. Note that we don't support
// non-null SocketTag.

TEST(HttpStreamKeyTest, Equality) {
  HttpStreamKey key(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true);

  EXPECT_EQ(key,
            HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/true));

  EXPECT_NE(key,
            HttpStreamKey(url::SchemeHostPort("https", "othersite", 443),
                          PRIVACY_MODE_DISABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/true));

  EXPECT_NE(key,
            HttpStreamKey(kHost, PRIVACY_MODE_ENABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/true));

  HttpStreamKey anonymized_key(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                               NetworkAnonymizationKey::CreateSameSite(
                                   SchemefulSite(GURL("http://a.test/"))),
                               SecureDnsPolicy::kAllow,
                               /*disable_cert_network_fetches=*/true);
  if (NetworkAnonymizationKey::IsPartitioningEnabled()) {
    EXPECT_NE(key, anonymized_key);
  } else {
    EXPECT_EQ(key, anonymized_key);
  }

  EXPECT_NE(key,
            HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                          /*disable_cert_network_fetches=*/true));

  EXPECT_NE(key,
            HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/false));
}

TEST(HttpStreamKeyTest, OrderedSet) {
  const std::vector<HttpStreamKey> stream_keys = {
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true),
      HttpStreamKey(url::SchemeHostPort("https", "othersite", 443),
                    PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true),
      HttpStreamKey(kHost, PRIVACY_MODE_ENABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true),
      // This has different network_anonymization_key, but it's the same as the
      // first one when anonymization is disabled.
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey::CreateSameSite(
                        SchemefulSite(GURL("http://a.test/"))),
                    SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true),
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                    /*disable_cert_network_fetches=*/true),
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/false),
  };

  const std::set<HttpStreamKey> key_set(stream_keys.begin(), stream_keys.end());
  const size_t expected_size = NetworkAnonymizationKey::IsPartitioningEnabled()
                                   ? stream_keys.size()
                                   : stream_keys.size() - 1;
  ASSERT_EQ(key_set.size(), expected_size);
}

TEST(HttpStreamKeyTest, Anonymization) {
  for (const bool enabled : {false, true}) {
    SCOPED_TRACE(enabled ? "Anonymization enabled" : "Anonymization disabled");

    base::test::ScopedFeatureList feature_list;
    if (enabled) {
      feature_list.InitAndEnableFeature(
          features::kPartitionConnectionsByNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kPartitionConnectionsByNetworkIsolationKey);
    }

    const HttpStreamKey key(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/true);

    const HttpStreamKey anonymized_key(
        kHost, PRIVACY_MODE_DISABLED, SocketTag(),
        NetworkAnonymizationKey::CreateSameSite(
            SchemefulSite(GURL("http://a.test/"))),
        SecureDnsPolicy::kAllow,
        /*disable_cert_network_fetches=*/true);

    if (enabled) {
      EXPECT_NE(key, anonymized_key);
    } else {
      EXPECT_EQ(key, anonymized_key);
    }
  }
}

TEST(HttpStreamKeyTest, ToSpdySessionKey) {
  const url::SchemeHostPort kHttpHost("http", "example.com", 80);
  const url::SchemeHostPort kHttpsHost("https", "example.com", 443);

  SpdySessionKey http_key =
      HttpStreamKey(kHttpHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true)
          .ToSpdySessionKey();
  ASSERT_TRUE(http_key.host_port_pair().IsEmpty());

  SpdySessionKey https_key =
      HttpStreamKey(kHttpsHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true)
          .ToSpdySessionKey();
  ASSERT_EQ(https_key,
            SpdySessionKey(HostPortPair::FromSchemeHostPort(kHttpsHost),
                           PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                           SessionUsage::kDestination, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                           /*disable_cert_verification_network_fetches=*/true));
}

TEST(HttpStreamKeyTest, ToQuicSessionKey) {
  const url::SchemeHostPort kHttpHost("http", "example.com", 80);
  const url::SchemeHostPort kHttpsHost("https", "example.com", 443);

  QuicSessionKey http_key =
      HttpStreamKey(kHttpHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true)
          .ToQuicSessionKey();
  ASSERT_TRUE(http_key.host().empty());

  QuicSessionKey https_key =
      HttpStreamKey(kHttpsHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true)
          .ToQuicSessionKey();
  ASSERT_EQ(https_key,
            QuicSessionKey(HostPortPair::FromSchemeHostPort(kHttpsHost),
                           PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                           SessionUsage::kDestination, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                           /*require_dns_https_alpn=*/false));
}

}  // namespace net
