// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/preflight_cache.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
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

  void AppendEntry(const std::string& origin, const GURL& url) {
    cache_.AppendEntry(origin, url, CreateEntry());
  }

  bool CheckEntryAndRefreshCache(const std::string& origin, const GURL& url) {
    return cache_.CheckIfRequestCanSkipPreflight(
        origin, url, network::mojom::CredentialsMode::kInclude, "POST",
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
  const std::string origin("null");
  const std::string other_origin("http://www.other.com:80");
  const GURL url("http://www.test.com/A");
  const GURL other_url("http://www.test.com/B");

  EXPECT_EQ(0u, CountEntries());

  AppendEntry(origin, url);

  EXPECT_EQ(1u, CountEntries());

  AppendEntry(origin, other_url);

  EXPECT_EQ(2u, CountEntries());

  AppendEntry(other_origin, url);

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
  const std::string origin("null");
  const GURL url("http://www.test.com/A");
  const GURL other_url("http://www.test.com/B");

  EXPECT_EQ(0u, CountEntries());

  AppendEntry(origin, url);
  AppendEntry(origin, other_url);

  EXPECT_EQ(2u, CountEntries());

  // Cache entry should still be valid.
  EXPECT_TRUE(CheckEntryAndRefreshCache(origin, url));

  // Advance time by ten seconds.
  Advance(10);

  // Cache entry should now be expired.
  EXPECT_FALSE(CheckEntryAndRefreshCache(origin, url));

  EXPECT_EQ(1u, CountEntries());

  // Cache entry should be expired.
  EXPECT_FALSE(CheckEntryAndRefreshCache(origin, other_url));

  EXPECT_EQ(0u, CountEntries());
}

}  // namespace

}  // namespace cors

}  // namespace network
