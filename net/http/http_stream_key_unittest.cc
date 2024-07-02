// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_key.h"

#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/socket_tag.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

// These tests are similar to SpdySessionKeyTest. Note that we don't support
// non-null SocketTag.

TEST(HttpStreamKeyTest, Equality) {
  const url::SchemeHostPort kHost("https", "www.example.com", 443);

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

  EXPECT_NE(key, HttpStreamKey(kHost, PRIVACY_MODE_DISABLED, SocketTag(),
                               NetworkAnonymizationKey::CreateSameSite(
                                   SchemefulSite(GURL("http://a.test/"))),
                               SecureDnsPolicy::kAllow,
                               /*disable_cert_network_fetches=*/true));

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
  const url::SchemeHostPort kHost("https", "www.example.com", 443);

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
  ASSERT_EQ(stream_keys.size(), key_set.size());
}

}  // namespace net
