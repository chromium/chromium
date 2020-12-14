// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

TEST(ClientSocketPool, GroupIdOperators) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  // Each of these lists is in "<" order, as defined by Group::operator< on the
  // corresponding field.

  // HostPortPair::operator< compares port before host.
  const HostPortPair kHostPortPairs[] = {
      {"b", 79}, {"a", 80}, {"b", 80}, {"c", 81}, {"a", 443}, {"c", 443},
  };

  const ClientSocketPool::SocketType kSocketTypes[] = {
      ClientSocketPool::SocketType::kHttp,
      ClientSocketPool::SocketType::kSsl,
  };

  const PrivacyMode kPrivacyModes[] = {
      PrivacyMode::PRIVACY_MODE_DISABLED,
      PrivacyMode::PRIVACY_MODE_ENABLED,
  };

  const SchemefulSite kSiteA(GURL("http://a.test/"));
  const SchemefulSite kSiteB(GURL("http://b.test/"));
  const NetworkIsolationKey kNetworkIsolationKeys[] = {
      NetworkIsolationKey(kSiteA, kSiteA),
      NetworkIsolationKey(kSiteB, kSiteB),
  };

  const bool kDisableSecureDnsValues[] = {false, true};

  // All previously created |group_ids|. They should all be less than the
  // current group under consideration.
  std::vector<ClientSocketPool::GroupId> group_ids;

  // Iterate through all sets of group ids, from least to greatest.
  for (const auto& host_port_pair : kHostPortPairs) {
    SCOPED_TRACE(host_port_pair.ToString());
    for (const auto& socket_type : kSocketTypes) {
      SCOPED_TRACE(static_cast<int>(socket_type));
      for (const auto& privacy_mode : kPrivacyModes) {
        SCOPED_TRACE(privacy_mode);
        for (const auto& network_isolation_key : kNetworkIsolationKeys) {
          SCOPED_TRACE(network_isolation_key.ToString());
          for (const auto& disable_secure_dns : kDisableSecureDnsValues) {
            ClientSocketPool::GroupId group_id(
                host_port_pair, socket_type, privacy_mode,
                network_isolation_key, disable_secure_dns);
            for (const auto& lower_group_id : group_ids) {
              EXPECT_FALSE(lower_group_id == group_id);
              EXPECT_TRUE(lower_group_id < group_id);
              EXPECT_FALSE(group_id < lower_group_id);
            }

            group_ids.push_back(group_id);

            // Compare |group_id| to itself. Use two different copies of
            // |group_id|'s value, since to protect against bugs where an object
            // only equals itself.
            EXPECT_TRUE(group_ids.back() == group_id);
            EXPECT_FALSE(group_ids.back() < group_id);
            EXPECT_FALSE(group_id < group_ids.back());
          }
        }
      }
    }
  }
}

TEST(ClientSocketPool, GroupIdToString) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kPartitionConnectionsByNetworkIsolationKey},
      {features::kAppendFrameOriginToNetworkIsolationKey});

  EXPECT_EQ("foo:80 <null>",
            ClientSocketPool::GroupId(
                HostPortPair("foo", 80), ClientSocketPool::SocketType::kHttp,
                PrivacyMode::PRIVACY_MODE_DISABLED, NetworkIsolationKey(),
                false /* disable_secure_dns */)
                .ToString());
  EXPECT_EQ("bar:443 <null>",
            ClientSocketPool::GroupId(
                HostPortPair("bar", 443), ClientSocketPool::SocketType::kHttp,
                PrivacyMode::PRIVACY_MODE_DISABLED, NetworkIsolationKey(),
                false /* disable_secure_dns */)
                .ToString());
  EXPECT_EQ("pm/bar:80 <null>",
            ClientSocketPool::GroupId(
                HostPortPair("bar", 80), ClientSocketPool::SocketType::kHttp,
                PrivacyMode::PRIVACY_MODE_ENABLED, NetworkIsolationKey(),
                false /* disable_secure_dns */)
                .ToString());

  EXPECT_EQ("ssl/foo:80 <null>",
            ClientSocketPool::GroupId(
                HostPortPair("foo", 80), ClientSocketPool::SocketType::kSsl,
                PrivacyMode::PRIVACY_MODE_DISABLED, NetworkIsolationKey(),
                false /* disable_secure_dns */)
                .ToString());
  EXPECT_EQ("ssl/bar:443 <null>",
            ClientSocketPool::GroupId(
                HostPortPair("bar", 443), ClientSocketPool::SocketType::kSsl,
                PrivacyMode::PRIVACY_MODE_DISABLED, NetworkIsolationKey(),
                false /* disable_secure_dns */)
                .ToString());
  EXPECT_EQ("pm/ssl/bar:80 <null>",
            ClientSocketPool::GroupId(
                HostPortPair("bar", 80), ClientSocketPool::SocketType::kSsl,
                PrivacyMode::PRIVACY_MODE_ENABLED, NetworkIsolationKey(),
                false /* disable_secure_dns */)
                .ToString());

  EXPECT_EQ("ssl/foo:443 <https://foo.com>",
            ClientSocketPool::GroupId(
                HostPortPair("foo", 443), ClientSocketPool::SocketType::kSsl,
                PrivacyMode::PRIVACY_MODE_DISABLED,
                NetworkIsolationKey(SchemefulSite(GURL("https://foo.com")),
                                    SchemefulSite(GURL("https://foo.com"))),
                false /* disable_secure_dns */)
                .ToString());

  EXPECT_EQ("dsd/pm/ssl/bar:80 <null>",
            ClientSocketPool::GroupId(
                HostPortPair("bar", 80), ClientSocketPool::SocketType::kSsl,
                PrivacyMode::PRIVACY_MODE_ENABLED, NetworkIsolationKey(),
                true /* disable_secure_dns */)
                .ToString());
}

TEST(ClientSocketPool, PartitionConnectionsByNetworkIsolationKeyDisabled) {
  const SchemefulSite kSiteFoo(GURL("https://foo.com"));
  const SchemefulSite kSiteBar(GURL("https://bar.com"));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  ClientSocketPool::GroupId group_id1(
      HostPortPair("foo", 443), ClientSocketPool::SocketType::kSsl,
      PrivacyMode::PRIVACY_MODE_DISABLED,
      NetworkIsolationKey(kSiteFoo, kSiteFoo), false /* disable_secure_dns */);

  ClientSocketPool::GroupId group_id2(
      HostPortPair("foo", 443), ClientSocketPool::SocketType::kSsl,
      PrivacyMode::PRIVACY_MODE_DISABLED,
      NetworkIsolationKey(kSiteBar, kSiteBar), false /* disable_secure_dns */);

  EXPECT_FALSE(group_id1.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(group_id2.network_isolation_key().IsFullyPopulated());
  EXPECT_EQ(group_id1.network_isolation_key(),
            group_id2.network_isolation_key());
  EXPECT_EQ(group_id1, group_id2);

  EXPECT_EQ("ssl/foo:443", group_id1.ToString());
  EXPECT_EQ("ssl/foo:443", group_id2.ToString());
}

}  // namespace

}  // namespace net
