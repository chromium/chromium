// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/agent_cluster_key.h"

#include <sstream>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

using AgentClusterKeyTest = testing::Test;

TEST_F(AgentClusterKeyTest, SiteKeyed) {
  GURL url = GURL("https://a.com");
  AgentClusterKey key = AgentClusterKey::CreateSiteKeyed(url);
  EXPECT_TRUE(key.IsSiteKeyed());
  EXPECT_FALSE(key.IsOriginKeyed());
  EXPECT_EQ(url, key.GetSite());
  EXPECT_EQ(std::nullopt, key.GetCrossOriginIsolationKey());
  ASSERT_DCHECK_DEATH(key.GetOrigin());
}

TEST_F(AgentClusterKeyTest, OriginKeyed) {
  url::Origin origin =
      url::Origin::CreateFromNormalizedTuple("https", "example.com", 443);
  AgentClusterKey key = AgentClusterKey::CreateOriginKeyed(origin);
  EXPECT_FALSE(key.IsSiteKeyed());
  EXPECT_TRUE(key.IsOriginKeyed());
  EXPECT_EQ(origin, key.GetOrigin());
  EXPECT_EQ(std::nullopt, key.GetCrossOriginIsolationKey());
  ASSERT_DCHECK_DEATH(key.GetSite());
}

TEST_F(AgentClusterKeyTest, WithCrossOriginIsolationKey) {
  url::Origin origin =
      url::Origin::CreateFromNormalizedTuple("https", "example.com", 443);
  url::Origin common_coi_origin = url::Origin::CreateFromNormalizedTuple(
      "https", "isolation.example.com", 443);
  AgentClusterKey::CrossOriginIsolationKey isolation_key(
      common_coi_origin, CrossOriginIsolationMode::kConcrete);
  AgentClusterKey key =
      AgentClusterKey::CreateWithCrossOriginIsolationKey(origin, isolation_key);

  EXPECT_FALSE(key.IsSiteKeyed());
  EXPECT_TRUE(key.IsOriginKeyed());
  EXPECT_EQ(origin, key.GetOrigin());
  EXPECT_EQ(isolation_key, key.GetCrossOriginIsolationKey());
  ASSERT_DCHECK_DEATH(key.GetSite());
}

TEST_F(AgentClusterKeyTest, Comparisons) {
  // Site-keyed
  GURL site_a = GURL("https://a.com");
  GURL site_b = GURL("https://b.com");

  AgentClusterKey key_site_a = AgentClusterKey::CreateSiteKeyed(site_a);
  AgentClusterKey key_site_b = AgentClusterKey::CreateSiteKeyed(site_b);

  EXPECT_EQ(key_site_a, key_site_a);
  EXPECT_NE(key_site_a, key_site_b);

  // Origin-keyed
  url::Origin origin_a = url::Origin::Create(site_a);
  url::Origin origin_b = url::Origin::Create(site_b);

  AgentClusterKey key_origin_a = AgentClusterKey::CreateOriginKeyed(origin_a);
  AgentClusterKey key_origin_b = AgentClusterKey::CreateOriginKeyed(origin_b);

  EXPECT_EQ(key_origin_a, key_origin_a);
  EXPECT_NE(key_origin_a, key_origin_b);
  EXPECT_NE(key_origin_a, key_site_a);

  // With isolation key
  AgentClusterKey::CrossOriginIsolationKey coi_a(
      origin_a, CrossOriginIsolationMode::kConcrete);
  AgentClusterKey::CrossOriginIsolationKey coi_b(
      origin_b, CrossOriginIsolationMode::kConcrete);
  AgentClusterKey::CrossOriginIsolationKey non_coi_a(
      origin_a, CrossOriginIsolationMode::kLogical);
  AgentClusterKey::CrossOriginIsolationKey non_coi_b(
      origin_b, CrossOriginIsolationMode::kLogical);

  EXPECT_EQ(coi_a, coi_a);
  EXPECT_EQ(non_coi_a, non_coi_a);
  EXPECT_NE(coi_a, coi_b);
  EXPECT_NE(coi_a, non_coi_a);
  EXPECT_NE(non_coi_a, non_coi_b);

  AgentClusterKey key_origin_a_coi_a =
      AgentClusterKey::CreateWithCrossOriginIsolationKey(origin_a, coi_a);
  AgentClusterKey key_origin_b_coi_a =
      AgentClusterKey::CreateWithCrossOriginIsolationKey(origin_b, coi_a);
  AgentClusterKey key_origin_a_coi_b =
      AgentClusterKey::CreateWithCrossOriginIsolationKey(origin_a, coi_b);
  AgentClusterKey key_origin_a_non_coi_a =
      AgentClusterKey::CreateWithCrossOriginIsolationKey(origin_a, non_coi_a);

  EXPECT_EQ(key_origin_a_coi_a, key_origin_a_coi_a);
  EXPECT_NE(key_origin_a_coi_a, key_origin_b_coi_a);
  EXPECT_NE(key_origin_a_coi_a, key_origin_a_coi_b);
  EXPECT_NE(key_origin_a_coi_a, key_origin_a_non_coi_a);
  EXPECT_NE(key_origin_a_coi_a, key_origin_a);
  EXPECT_NE(key_origin_a_coi_a, key_site_a);
  EXPECT_NE(key_origin_a_non_coi_a, key_origin_a);
  EXPECT_NE(key_origin_a_non_coi_a, key_site_a);
}

TEST_F(AgentClusterKeyTest, StreamOutput) {
  std::stringstream dump;
  GURL url_a("https://a.com");
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(GURL("https://b.com"));

  AgentClusterKey key_site_a = AgentClusterKey::CreateSiteKeyed(url_a);
  dump << key_site_a;
  EXPECT_EQ(dump.str(), "{site_: https://a.com/}");
  dump.str("");

  AgentClusterKey key_origin_a = AgentClusterKey::CreateOriginKeyed(origin_a);
  dump << key_origin_a;
  EXPECT_EQ(dump.str(), "{origin_: https://a.com}");
  dump.str("");

  AgentClusterKey key_origin_a_coi_b =
      AgentClusterKey::CreateWithCrossOriginIsolationKey(
          origin_a, AgentClusterKey::CrossOriginIsolationKey(
                        origin_b, CrossOriginIsolationMode::kConcrete));
  dump << key_origin_a_coi_b;
  EXPECT_EQ(dump.str(),
            "{origin_: https://a.com, cross_origin_isolation_key_: "
            "{common_coi_origin: "
            "https://b.com, cross_origin_isolation_mode: concrete}}");
  dump.str("");
}

}  // namespace content
