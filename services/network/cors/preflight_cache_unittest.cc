// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_cache.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace cors {

namespace {

class PreflightCacheTest : public testing::Test {
 public:
  PreflightCacheTest() = default;

 protected:
  size_t CountEntries() const { return cache_.CountEntriesForTesting(); }
  void MayPurge(size_t max_entries, size_t purge_unit) {
    cache_.MayPurgeForTesting(max_entries, purge_unit);
  }
  PreflightCache* cache() { return &cache_; }

  std::unique_ptr<PreflightResult> CreateEntry() {
    return PreflightResult::Create(mojom::CredentialsMode::kInclude,
                                   std::string("POST"), base::nullopt,
                                   std::string("5"), nullptr);
  }

  void AppendEntry(const url::Origin& origin,
                   const GURL& url,
                   const net::NetworkIsolationKey& network_isolation_key) {
    cache_.AppendEntry(origin, url, network_isolation_key, CreateEntry());
  }

  bool CheckEntryAndRefreshCache(
      const url::Origin& origin,
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key) {
    return cache_.CheckIfRequestCanSkipPreflight(
        origin, url, network_isolation_key,
        network::mojom::CredentialsMode::kInclude, "POST",
        net::HttpRequestHeaders(), false);
  }

  void Advance(int seconds) {
    clock_.Advance(base::TimeDelta::FromSeconds(seconds));
  }

 private:
  // testing::Test implementation.
  void SetUp() override { PreflightResult::SetTickClockForTesting(&clock_); }
  void TearDown() override { PreflightResult::SetTickClockForTesting(nullptr); }

  base::test::TaskEnvironment env_;
  PreflightCache cache_;
  base::SimpleTestTickClock clock_;
};

TEST_F(PreflightCacheTest, CacheSize) {
  const url::Origin origin;
  const url::Origin other_origin =
      url::Origin::Create(GURL("http://www.other.com:80"));
  const GURL url("http://www.test.com/A");
  const GURL other_url("http://www.test.com/B");

  EXPECT_EQ(0u, CountEntries());

  AppendEntry(origin, url, net::NetworkIsolationKey());

  EXPECT_EQ(1u, CountEntries());

  AppendEntry(origin, other_url, net::NetworkIsolationKey());

  EXPECT_EQ(2u, CountEntries());

  AppendEntry(other_origin, url, net::NetworkIsolationKey());

  EXPECT_EQ(3u, CountEntries());

  // Num of entries is 3, that is not greater than the limit 3u.
  // It results in doing nothing.
  MayPurge(3u, 2u);
  EXPECT_EQ(3u, CountEntries());

  // Num of entries is 3, that is greater than the limit 2u.
  // It results in purging entries by the specified unit 2u, thus only one entry
  // remains.
  MayPurge(2u, 2u);
  EXPECT_EQ(1u, CountEntries());

  // This will make the cache empty. Note that the cache expects the num of
  // remaining entries should be greater than the specified purge unit.
  MayPurge(0u, 1u);
  EXPECT_EQ(0u, CountEntries());
}

TEST_F(PreflightCacheTest, CacheTimeout) {
  const url::Origin origin;
  const GURL url("http://www.test.com/A");
  const GURL other_url("http://www.test.com/B");

  EXPECT_EQ(0u, CountEntries());

  AppendEntry(origin, url, net::NetworkIsolationKey());
  AppendEntry(origin, other_url, net::NetworkIsolationKey());

  EXPECT_EQ(2u, CountEntries());

  // Cache entry should still be valid.
  EXPECT_TRUE(
      CheckEntryAndRefreshCache(origin, url, net::NetworkIsolationKey()));

  // Advance time by ten seconds.
  Advance(10);

  // Cache entry should now be expired.
  EXPECT_FALSE(
      CheckEntryAndRefreshCache(origin, url, net::NetworkIsolationKey()));

  EXPECT_EQ(1u, CountEntries());

  // Cache entry should be expired.
  EXPECT_FALSE(
      CheckEntryAndRefreshCache(origin, other_url, net::NetworkIsolationKey()));

  EXPECT_EQ(0u, CountEntries());
}

TEST_F(PreflightCacheTest, RespectsNetworkIsolationKeys) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kAppendFrameOriginToNetworkIsolationKey);

  const std::string kOriginStr1("http://www.test.com/A");
  const url::Origin kOrigin1 = url::Origin::Create(GURL(kOriginStr1));
  const net::NetworkIsolationKey kNik(kOrigin1, kOrigin1);
  const GURL kUrl1(kOriginStr1);

  const GURL kUrl2("http://www.other.com:80");

  // The cache starts empty.
  EXPECT_EQ(0u, CountEntries());

  AppendEntry(kOrigin1, kUrl1, net::NetworkIsolationKey());
  EXPECT_EQ(1u, CountEntries());

  // This should be indistinguishable from the previous key, so it should not
  // increase the size of the cache.
  AppendEntry(kOrigin1, kUrl1, net::NetworkIsolationKey());
  EXPECT_EQ(1u, CountEntries());
  EXPECT_TRUE(
      CheckEntryAndRefreshCache(kOrigin1, kUrl1, net::NetworkIsolationKey()));
  EXPECT_FALSE(CheckEntryAndRefreshCache(kOrigin1, kUrl1, kNik));

  // This uses a different NIK, so it should result in a new entry in the cache.
  AppendEntry(kOrigin1, kUrl1, kNik);
  EXPECT_EQ(2u, CountEntries());
  EXPECT_TRUE(CheckEntryAndRefreshCache(kOrigin1, kUrl1, kNik));

  // Check that an entry we never inserted is not found in the cache.
  EXPECT_FALSE(CheckEntryAndRefreshCache(kOrigin1, kUrl2, kNik));
}

TEST_F(PreflightCacheTest, HandlesOpaqueOrigins) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kAppendFrameOriginToNetworkIsolationKey);

  const url::Origin kOrigin1;
  const url::Origin kOrigin2;
  const net::NetworkIsolationKey kNik1(kOrigin1, kOrigin1);
  const net::NetworkIsolationKey kNik2(kOrigin2, kOrigin2);
  const GURL kUrl("http://www.test.com/A");

  // The cache starts empty.
  EXPECT_EQ(0u, CountEntries());

  AppendEntry(kOrigin1, kUrl, kNik1);
  EXPECT_EQ(1u, CountEntries());
  EXPECT_TRUE(CheckEntryAndRefreshCache(kOrigin1, kUrl, kNik1));

  // The cache should report a miss if we use a new opaque origin and the same
  // URL and NIK, since the nonces of the origins differ.
  EXPECT_FALSE(CheckEntryAndRefreshCache(url::Origin(), kUrl, kNik1));

  // This should be distinguishable from the previous NIK, so it should
  // increase the size of the cache.
  AppendEntry(kOrigin1, kUrl, kNik2);
  EXPECT_EQ(2u, CountEntries());
  EXPECT_TRUE(CheckEntryAndRefreshCache(kOrigin1, kUrl, kNik2));
  EXPECT_FALSE(
      CheckEntryAndRefreshCache(kOrigin1, kUrl, net::NetworkIsolationKey()));
  EXPECT_FALSE(CheckEntryAndRefreshCache(
      kOrigin1, kUrl, net::NetworkIsolationKey(url::Origin(), url::Origin())));
}

}  // namespace

}  // namespace cors

}  // namespace network
