// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/prefetch_cache.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/prefetch_url_loader_client.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

constexpr size_t kMaxSize = 10;

// These Test* functions return a value of the appropriate type useful for
// testing. By passing in an index, you can make them produce different
// hostnames that are not same-origin with each other.
GURL TestURL(int index = 0) {
  return GURL(base::StringPrintf("https://origin%d.example/i.js", index));
}

url::Origin TestOrigin(int index = 0) {
  return url::Origin::Create(TestURL(index));
}

net::IsolationInfo TestIsolationInfo(int index = 0) {
  return net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, TestOrigin(index),
      TestOrigin(index), net::SiteForCookies::FromOrigin(TestOrigin(index)));
}

net::NetworkIsolationKey TestNIK(int index = 0) {
  const net::NetworkIsolationKey nik =
      TestIsolationInfo(index).network_isolation_key();
  return nik;
}

ResourceRequest MakeResourceRequest(GURL url,
                                    net::IsolationInfo isolation_info) {
  ResourceRequest request;
  request.url = std::move(url);
  request.trusted_params.emplace();
  request.trusted_params->isolation_info = std::move(isolation_info);
  return request;
}

class PrefetchCacheTest : public ::testing::Test {
 protected:
  PrefetchCacheTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kNetworkContextPrefetch,
        {{"max_loaders", base::NumberToString(kMaxSize)}});
  }

  PrefetchCache& cache() { return cache_; }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  PrefetchCache cache_;
};

TEST_F(PrefetchCacheTest, Emplace) {
  PrefetchURLLoaderClient* client =
      cache().Emplace(MakeResourceRequest(TestURL(), TestIsolationInfo()));
  ASSERT_TRUE(client);
  EXPECT_EQ(client->url(), TestURL());
  EXPECT_EQ(client->network_isolation_key(), TestNIK());
}

TEST_F(PrefetchCacheTest, EmplaceNoNIK) {
  ResourceRequest request;
  request.url = TestURL();
  // This will log a warning when debug logging is enabled but it is harmless.
  EXPECT_FALSE(cache().Emplace(request));
}

TEST_F(PrefetchCacheTest, EmplaceTransientNIK) {
  // This will log a warning when debug logging is enabled but it is harmless.
  EXPECT_FALSE(cache().Emplace(
      MakeResourceRequest(TestURL(), net::IsolationInfo::CreateTransient())));
}

TEST_F(PrefetchCacheTest, EmplaceFileURL) {
  // This will log a warning when debug logging is enabled but it is harmless.
  EXPECT_FALSE(cache().Emplace(
      MakeResourceRequest(GURL("file:///tmp/bogus.js"), TestIsolationInfo())));
}

TEST_F(PrefetchCacheTest, EmplaceSameURLNIK) {
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(), TestIsolationInfo())));
  EXPECT_FALSE(
      cache().Emplace(MakeResourceRequest(TestURL(), TestIsolationInfo())));
}

TEST_F(PrefetchCacheTest, EmplaceDifferentURLSameNIK) {
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(0), TestIsolationInfo())));
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(1), TestIsolationInfo())));
}

TEST_F(PrefetchCacheTest, EmplaceSameURLDifferentNIK) {
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(), TestIsolationInfo(0))));
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(), TestIsolationInfo(1))));
}

TEST_F(PrefetchCacheTest, SuccessfulLookup) {
  PrefetchURLLoaderClient* emplaced_client =
      cache().Emplace(MakeResourceRequest(TestURL(), TestIsolationInfo()));
  EXPECT_TRUE(emplaced_client);
  PrefetchURLLoaderClient* retrieved_client =
      cache().Lookup(TestNIK(), TestURL());

  ASSERT_TRUE(retrieved_client);
  EXPECT_EQ(emplaced_client, retrieved_client);
  EXPECT_EQ(retrieved_client->url(), TestURL());
  EXPECT_EQ(retrieved_client->network_isolation_key(), TestNIK());
}

TEST_F(PrefetchCacheTest, FailedLookup) {
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(1), TestIsolationInfo(1))));
  EXPECT_FALSE(cache().Lookup(TestNIK(2), TestURL(2)));
}

TEST_F(PrefetchCacheTest, EmplaceRespectsMaxSize) {
  // Insert kMaxSize distinct items into the cache.
  for (size_t i = 0; i < kMaxSize; ++i) {
    EXPECT_TRUE(
        cache().Emplace(MakeResourceRequest(TestURL(i), TestIsolationInfo())));
  }

  // Verify they are all still there.
  for (size_t i = 0; i < kMaxSize; ++i) {
    EXPECT_TRUE(cache().Lookup(TestNIK(), TestURL(i)));
  }

  // Add another item.
  EXPECT_TRUE(cache().Emplace(
      MakeResourceRequest(TestURL(kMaxSize), TestIsolationInfo())));

  // The oldest one should now be gone.
  EXPECT_FALSE(cache().Lookup(TestNIK(), TestURL(0)));

  // The rest should still be there.
  for (size_t i = 1; i < kMaxSize + 1; ++i) {
    EXPECT_TRUE(cache().Lookup(TestNIK(), TestURL(i)));
  }
}

TEST_F(PrefetchCacheTest, ConsumedNodesNoLongerInCache) {
  auto* client =
      cache().Emplace(MakeResourceRequest(TestURL(), TestIsolationInfo()));
  cache().Consume(client);
  EXPECT_FALSE(cache().Lookup(TestNIK(), TestURL()));
  cache().Erase(client);
}

TEST_F(PrefetchCacheTest, ConsumedNodesDontCountTowardsCacheSize) {
  // Insert kMaxSize distinct items into the cache.
  for (size_t i = 0; i < kMaxSize; ++i) {
    EXPECT_TRUE(
        cache().Emplace(MakeResourceRequest(TestURL(i), TestIsolationInfo())));
  }

  // Consume one of them.
  auto* client = cache().Lookup(TestNIK(), TestURL(kMaxSize - 1));
  cache().Consume(client);

  // Insert a new item into the cache.
  EXPECT_TRUE(cache().Emplace(
      MakeResourceRequest(TestURL(kMaxSize), TestIsolationInfo())));

  // Check the remaining items are all still there.
  for (size_t i = 0; i < kMaxSize - 1; ++i) {
    EXPECT_TRUE(cache().Lookup(TestNIK(), TestURL(i)));
  }

  cache().Erase(client);
}

TEST_F(PrefetchCacheTest, ExpiryHappens) {
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(), TestIsolationInfo())));
  FastForwardBy(PrefetchCache::kMaxAge);
  EXPECT_FALSE(cache().Lookup(TestNIK(), TestURL()));
}

// Prefetches that are submitted close together are expired from the cache
// together, to reduce the number of unnecessary wake-ups.
TEST_F(PrefetchCacheTest, TimerSlackIsApplied) {
  const auto kTimeBetweenPrefetches = base::Milliseconds(1);
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(0), TestIsolationInfo())));
  FastForwardBy(kTimeBetweenPrefetches);
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(1), TestIsolationInfo())));

  FastForwardBy(PrefetchCache::kMaxAge - kTimeBetweenPrefetches);

  EXPECT_FALSE(cache().Lookup(TestNIK(), TestURL(0)));
  EXPECT_FALSE(cache().Lookup(TestNIK(), TestURL(1)));
}

TEST_F(PrefetchCacheTest, SeparatedPrefetchesExpiredSeparatedly) {
  const auto kPrefetchSeparation = PrefetchCache::kMaxAge / 2;
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(0), TestIsolationInfo())));
  FastForwardBy(kPrefetchSeparation);
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(1), TestIsolationInfo())));

  FastForwardBy(PrefetchCache::kMaxAge - kPrefetchSeparation);

  EXPECT_FALSE(cache().Lookup(TestNIK(), TestURL(0)));
  EXPECT_TRUE(cache().Lookup(TestNIK(), TestURL(1)));

  FastForwardBy(kPrefetchSeparation);
  EXPECT_FALSE(cache().Lookup(TestNIK(), TestURL(1)));
}

TEST_F(PrefetchCacheTest, ConsumedPrefetchesAreNotExpired) {
  auto* client =
      cache().Emplace(MakeResourceRequest(TestURL(0), TestIsolationInfo()));
  ASSERT_TRUE(client);
  FastForwardBy(PrefetchCache::kMaxAge / 2);
  EXPECT_TRUE(
      cache().Emplace(MakeResourceRequest(TestURL(1), TestIsolationInfo())));
  cache().Consume(client);
  FastForwardBy(PrefetchCache::kMaxAge);
  // An ASAN build would catch the error here if the client had been deleted.
  EXPECT_EQ(client->url(), TestURL(0));
  // The other entry still gets expired correctly.
  EXPECT_FALSE(cache().Lookup(TestNIK(), TestURL(1)));

  cache().Erase(client);
}

}  // namespace

}  // namespace network
