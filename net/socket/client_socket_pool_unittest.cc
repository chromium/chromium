// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/schemeful_site.h"
#include "net/dns/public/secure_dns_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

namespace {

TEST(ClientSocketPool, GroupIdOperators) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  // Each of these lists is in "<" order, as defined by Group::operator< on the
  // corresponding field.

  const uint16_t kPorts[] = {
      80,
      81,
      443,
  };

  const char* kSchemes[] = {
      url::kHttpScheme,
      url::kHttpsScheme,
  };

  const char* kHosts[] = {
      "a",
      "b",
      "c",
  };

  const PrivacyMode kPrivacyModes[] = {
      PrivacyMode::PRIVACY_MODE_DISABLED,
      PrivacyMode::PRIVACY_MODE_ENABLED,
  };

  const SchemefulSite kSiteA(GURL("http://a.test/"));
  const SchemefulSite kSiteB(GURL("http://b.test/"));
  const NetworkAnonymizationKey kNetworkAnonymizationKeys[] = {
      NetworkAnonymizationKey::CreateSameSite(kSiteA),
      NetworkAnonymizationKey::CreateSameSite(kSiteB),
  };

  const SecureDnsPolicy kDisableSecureDnsValues[] = {SecureDnsPolicy::kAllow,
                                                     SecureDnsPolicy::kDisable};

  // All previously created |group_ids|. They should all be less than the
  // current group under consideration.
  std::vector<ClientSocketPool::GroupId> group_ids;

  // Iterate through all sets of group ids, from least to greatest.
  for (const auto& port : kPorts) {
    SCOPED_TRACE(port);
    for (const char* scheme : kSchemes) {
      SCOPED_TRACE(scheme);
      for (const char* host : kHosts) {
        SCOPED_TRACE(host);
        for (const auto& privacy_mode : kPrivacyModes) {
          SCOPED_TRACE(privacy_mode);
          for (const auto& network_anonymization_key :
               kNetworkAnonymizationKeys) {
            SCOPED_TRACE(network_anonymization_key.ToDebugString());
            for (const auto& secure_dns_policy : kDisableSecureDnsValues) {
              ClientSocketPool::GroupId group_id(
                  url::SchemeHostPort(scheme, host, port), privacy_mode,
                  network_anonymization_key, secure_dns_policy,
                  /*disable_cert_network_fetches=*/false);
              for (const auto& lower_group_id : group_ids) {
                EXPECT_FALSE(lower_group_id == group_id);
                EXPECT_TRUE(lower_group_id < group_id);
                EXPECT_FALSE(group_id < lower_group_id);
              }

              group_ids.push_back(group_id);

              // Compare |group_id| to itself. Use two different copies of
              // |group_id|'s value, since to protect against bugs where an
              // object only equals itself.
              EXPECT_TRUE(group_ids.back() == group_id);
              EXPECT_FALSE(group_ids.back() < group_id);
              EXPECT_FALSE(group_id < group_ids.back());
            }
          }
        }
      }
    }
  }
}

TEST(ClientSocketPool, GroupIdToString) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  EXPECT_EQ("http://foo <null>",
            ClientSocketPool::GroupId(
                url::SchemeHostPort(url::kHttpScheme, "foo", 80),
                PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
                SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false)
                .ToString());
  EXPECT_EQ("http://bar:443 <null>",
            ClientSocketPool::GroupId(
                url::SchemeHostPort(url::kHttpScheme, "bar", 443),
                PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
                SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false)
                .ToString());
  EXPECT_EQ("pm/http://bar <null>",
            ClientSocketPool::GroupId(
                url::SchemeHostPort(url::kHttpScheme, "bar", 80),
                PrivacyMode::PRIVACY_MODE_ENABLED, NetworkAnonymizationKey(),
                SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false)
                .ToString());

  EXPECT_EQ("https://foo:80 <null>",
            ClientSocketPool::GroupId(
                url::SchemeHostPort(url::kHttpsScheme, "foo", 80),
                PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
                SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false)
                .ToString());
  EXPECT_EQ("https://bar <null>",
            ClientSocketPool::GroupId(
                url::SchemeHostPort(url::kHttpsScheme, "bar", 443),
                PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
                SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false)
                .ToString());
  EXPECT_EQ("pm/https://bar:80 <null>",
            ClientSocketPool::GroupId(
                url::SchemeHostPort(url::kHttpsScheme, "bar", 80),
                PrivacyMode::PRIVACY_MODE_ENABLED, NetworkAnonymizationKey(),
                SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false)
                .ToString());

  EXPECT_EQ("https://foo <https://foo.test cross_site>",
            ClientSocketPool::GroupId(
                url::SchemeHostPort(url::kHttpsScheme, "foo", 443),
                PrivacyMode::PRIVACY_MODE_DISABLED,
                NetworkAnonymizationKey::CreateCrossSite(
                    SchemefulSite(GURL("https://foo.test"))),
                SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false)
                .ToString());

  EXPECT_EQ(
      "dsd/pm/https://bar:80 <null>",
      ClientSocketPool::GroupId(
          url::SchemeHostPort(url::kHttpsScheme, "bar", 80),
          PrivacyMode::PRIVACY_MODE_ENABLED, NetworkAnonymizationKey(),
          SecureDnsPolicy::kDisable, /*disable_cert_network_fetches=*/false)
          .ToString());

  EXPECT_EQ("disable_cert_network_fetches/pm/https://bar:80 <null>",
            ClientSocketPool::GroupId(
                url::SchemeHostPort(url::kHttpsScheme, "bar", 80),
                PrivacyMode::PRIVACY_MODE_ENABLED, NetworkAnonymizationKey(),
                SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/true)
                .ToString());
}

TEST(ClientSocketPool, SplitHostCacheByNetworkIsolationKeyDisabled) {
  const SchemefulSite kSiteFoo(GURL("https://foo.com"));
  const SchemefulSite kSiteBar(GURL("https://bar.com"));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  ClientSocketPool::GroupId group_id1(
      url::SchemeHostPort(url::kHttpsScheme, "foo", 443),
      PrivacyMode::PRIVACY_MODE_DISABLED,
      NetworkAnonymizationKey::CreateSameSite(kSiteFoo),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);

  ClientSocketPool::GroupId group_id2(
      url::SchemeHostPort(url::kHttpsScheme, "foo", 443),
      PrivacyMode::PRIVACY_MODE_DISABLED,
      NetworkAnonymizationKey::CreateSameSite(kSiteBar),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);

  EXPECT_FALSE(group_id1.network_anonymization_key().IsFullyPopulated());
  EXPECT_FALSE(group_id2.network_anonymization_key().IsFullyPopulated());
  EXPECT_EQ(group_id1.network_anonymization_key(),
            group_id2.network_anonymization_key());
  EXPECT_EQ(group_id1, group_id2);

  EXPECT_EQ("https://foo", group_id1.ToString());
  EXPECT_EQ("https://foo", group_id2.ToString());
}

}  // namespace

}  // namespace net
