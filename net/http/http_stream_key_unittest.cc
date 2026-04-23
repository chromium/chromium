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
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle);

  EXPECT_EQ(key,
            HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/true,
                          handles::kInvalidNetworkHandle));

  EXPECT_NE(key,
            HttpStreamKey(url::SchemeHostPort("https", "othersite", 443),
                          PRIVACY_MODE_DISABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/true,
                          handles::kInvalidNetworkHandle));

  EXPECT_NE(key,
            HttpStreamKey(kHost, PRIVACY_MODE_ENABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/true,
                          handles::kInvalidNetworkHandle));

  HttpStreamKey anonymized_key(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                               NetworkAnonymizationKey::CreateSameSite(
                                   SchemefulSite(GURL("http://a.test/"))),
                               SecureDnsPolicy::kAllow,
                               /*disable_cert_network_fetches=*/true,
                               handles::kInvalidNetworkHandle);
  if (NetworkAnonymizationKey::IsPartitioningEnabled()) {
    EXPECT_NE(key, anonymized_key);
  } else {
    EXPECT_EQ(key, anonymized_key);
  }

  EXPECT_NE(key,
            HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                          /*disable_cert_network_fetches=*/true,
                          handles::kInvalidNetworkHandle));

  EXPECT_NE(key,
            HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/false,
                          handles::kInvalidNetworkHandle));
}

TEST(HttpStreamKeyTest, OrderedSet) {
  const std::vector<HttpStreamKey> stream_keys = {
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle),
      HttpStreamKey(url::SchemeHostPort("https", "othersite", 443),
                    PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle),
      HttpStreamKey(kHost, PRIVACY_MODE_ENABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle),
      // This has different network_anonymization_key, but it's the same as the
      // first one when anonymization is disabled.
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey::CreateSameSite(
                        SchemefulSite(GURL("http://a.test/"))),
                    SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle),
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle),
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/false,
                    handles::kInvalidNetworkHandle),
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true, 1),
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true, 2),
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
                            /*disable_cert_network_fetches=*/true,
                            handles::kInvalidNetworkHandle);

    const HttpStreamKey anonymized_key(
        kHost, PRIVACY_MODE_DISABLED, SocketTag(),
        NetworkAnonymizationKey::CreateSameSite(
            SchemefulSite(GURL("http://a.test/"))),
        SecureDnsPolicy::kAllow,
        /*disable_cert_network_fetches=*/true, handles::kInvalidNetworkHandle);

    if (enabled) {
      EXPECT_NE(key, anonymized_key);
    } else {
      EXPECT_EQ(key, anonymized_key);
    }
  }
}

TEST(HttpStreamKeyTest, TargetNetworkEquality) {
  const HttpStreamKey key(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/true,
                          handles::kInvalidNetworkHandle);

  const HttpStreamKey target_network_1_key(
      kHost, PRIVACY_MODE_DISABLED, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/true, 1);
  const HttpStreamKey target_network_2_key(
      kHost, PRIVACY_MODE_DISABLED, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/true, 2);

  EXPECT_NE(key, target_network_1_key);
  EXPECT_NE(target_network_1_key, target_network_2_key);
}

TEST(HttpStreamKeyTest, ToSpdySessionKey) {
  const url::SchemeHostPort kHttpHost("http", "example.com", 80);
  const url::SchemeHostPort kHttpsHost("https", "example.com", 443);

  SpdySessionKey http_key =
      HttpStreamKey(kHttpHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle)
          .CalculateSpdySessionKey();
  ASSERT_TRUE(http_key.host_port_pair().IsEmpty());

  SpdySessionKey https_key =
      HttpStreamKey(kHttpsHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle)
          .CalculateSpdySessionKey();
  ASSERT_EQ(https_key,
            SpdySessionKey(HostPortPair::FromSchemeHostPort(kHttpsHost),
                           PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                           SessionUsage::kDestination, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                           /*disable_cert_verification_network_fetches=*/true));
}

TEST(HttpStreamKeyTest, ToSpdySessionKeyWithTargetNetworkEquality) {
  const HttpStreamKey key(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                          /*disable_cert_network_fetches=*/true,
                          handles::kInvalidNetworkHandle);
  const HttpStreamKey target_network_1_key(
      kHost, PRIVACY_MODE_DISABLED, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/true, 1);
  const HttpStreamKey target_network_2_key(
      kHost, PRIVACY_MODE_DISABLED, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/true, 2);

  // TODO(https://crbug.com/495684670): Update these assertions to _NE once
  //  SpdySessionKey supports target_network.
  ASSERT_EQ(key.CalculateSpdySessionKey(),
            target_network_1_key.CalculateSpdySessionKey());
  ASSERT_EQ(target_network_1_key.CalculateSpdySessionKey(),
            target_network_2_key.CalculateSpdySessionKey());
}

TEST(HttpStreamKeyTest, CalculateQuicSessionAliasKey) {
  const url::SchemeHostPort kHttpHost("http", "example.com", 80);
  const url::SchemeHostPort kHttpsHost("https", "example.com", 443);
  const url::SchemeHostPort kHttpsAliasHost("https", "alt.example.com", 443);
  const AlternativeService kHttp2AltService(
      NextProto::kProtoHTTP2, kHttpsAliasHost.host(), kHttpsAliasHost.port());
  const AlternativeService kQuicAltService(
      NextProto::kProtoQUIC, kHttpsAliasHost.host(), kHttpsAliasHost.port());

  QuicSessionAliasKey http_key =
      HttpStreamKey(kHttpHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle)
          .CalculateQuicSessionAliasKey();
  EXPECT_TRUE(http_key.session_key().host().empty());
  EXPECT_FALSE(http_key.destination().IsValid());

  QuicSessionAliasKey https_key =
      HttpStreamKey(kHttpsHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle)
          .CalculateQuicSessionAliasKey();
  EXPECT_EQ(https_key.session_key(),
            QuicSessionKey(HostPortPair::FromSchemeHostPort(kHttpsHost),
                           PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                           SessionUsage::kDestination, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                           /*require_dns_https_alpn=*/false,
                           /*disable_cert_verification_network_fetches=*/true));
  EXPECT_EQ(https_key.destination(), kHttpsHost);

  // H2 alt services should result in empty QUIC session alias keys.
  QuicSessionAliasKey https_key_with_h2_alt_service =
      HttpStreamKey(kHttpsHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle, kHttp2AltService)
          .CalculateQuicSessionAliasKey();
  EXPECT_TRUE(https_key_with_h2_alt_service.session_key().host().empty());
  EXPECT_FALSE(https_key_with_h2_alt_service.destination().IsValid());

  QuicSessionAliasKey different_origin_key =
      HttpStreamKey(kHttpsHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle, kQuicAltService)
          .CalculateQuicSessionAliasKey();
  EXPECT_EQ(different_origin_key.session_key(),
            QuicSessionKey(HostPortPair::FromSchemeHostPort(kHttpsHost),
                           PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                           SessionUsage::kDestination, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                           /*require_dns_https_alpn=*/false,
                           /*disable_cert_verification_network_fetches=*/true));
  EXPECT_EQ(different_origin_key.destination(), kHttpsAliasHost);
}

TEST(HttpStreamKeyTest, CalculateQuicSessionAliasKeyTargetNetworkEquality) {
  QuicSessionAliasKey key =
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true,
                    handles::kInvalidNetworkHandle)
          .CalculateQuicSessionAliasKey();

  QuicSessionAliasKey target_network_1_key =
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true, 1)
          .CalculateQuicSessionAliasKey();
  QuicSessionAliasKey target_network_2_key =
      HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                    NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                    /*disable_cert_network_fetches=*/true, 2)
          .CalculateQuicSessionAliasKey();

  // TODO(https://crbug.com/495684670): Update these assertions to _NE once
  // QuicSessionAliasKey supports target_network.
  EXPECT_EQ(key, target_network_1_key);
  EXPECT_EQ(target_network_1_key, target_network_2_key);
}

}  // namespace net
