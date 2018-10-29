// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/preflight_cache.h"

#include "base/test/simple_test_tick_clock.h"
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
  size_t CountOrigins() const { return cache_.CountOriginsForTesting(); }
  size_t CountEntries() const { return cache_.CountEntriesForTesting(); }
  PreflightCache* cache() { return &cache_; }

  void AppendEntry(const std::string& origin, const GURL& url) {
    std::unique_ptr<PreflightResult> result = PreflightResult::Create(
        mojom::FetchCredentialsMode::kInclude, std::string("POST"),
        base::nullopt, std::string("5"), nullptr);
    cache_.AppendEntry(origin, url, std::move(result));
  }

  bool CheckEntryAndRefreshCache(const std::string& origin, const GURL& url) {
    return cache_.CheckIfRequestCanSkipPreflight(
        origin, url, network::mojom::FetchCredentialsMode::kInclude, "POST",
        net::HttpRequestHeaders(), false);
  }

  void Advance(int seconds) {
    clock_.Advance(base::TimeDelta::FromSeconds(seconds));
  }

 private:
  // testing::Test implementation.
  void SetUp() override { PreflightResult::SetTickClockForTesting(&clock_); }
  void TearDown() override { PreflightResult::SetTickClockForTesting(nullptr); }

  PreflightCache cache_;
  base::SimpleTestTickClock clock_;
};

TEST_F(PreflightCacheTest, CacheSize) {
  const std::string origin("null");
  const std::string other_origin("http://www.other.com:80");
  const GURL url("http://www.test.com/A");
  const GURL other_url("http://www.test.com/B");

  EXPECT_EQ(0u, CountOrigins());
  EXPECT_EQ(0u, CountEntries());

  AppendEntry(origin, url);

  EXPECT_EQ(1u, CountOrigins());
  EXPECT_EQ(1u, CountEntries());

  AppendEntry(origin, other_url);

  EXPECT_EQ(1u, CountOrigins());
  EXPECT_EQ(2u, CountEntries());

  AppendEntry(other_origin, url);

  EXPECT_EQ(2u, CountOrigins());
  EXPECT_EQ(3u, CountEntries());
}

TEST_F(PreflightCacheTest, CacheTimeout) {
  const std::string origin("null");
  const GURL url("http://www.test.com/A");
  const GURL other_url("http://www.test.com/B");

  EXPECT_EQ(0u, CountOrigins());
  EXPECT_EQ(0u, CountEntries());

  AppendEntry(origin, url);
  AppendEntry(origin, other_url);

  EXPECT_EQ(1u, CountOrigins());
  EXPECT_EQ(2u, CountEntries());

  // Cache entry should still be valid.
  EXPECT_TRUE(CheckEntryAndRefreshCache(origin, url));

  // Advance time by ten seconds.
  Advance(10);

  // Cache entry should now be expired.
  EXPECT_FALSE(CheckEntryAndRefreshCache(origin, url));

  EXPECT_EQ(1u, CountOrigins());
  EXPECT_EQ(1u, CountEntries());

  // Cache entry should be expired.
  EXPECT_FALSE(CheckEntryAndRefreshCache(origin, other_url));

  EXPECT_EQ(0u, CountOrigins());
  EXPECT_EQ(0u, CountEntries());
}

}  // namespace

}  // namespace cors

}  // namespace network
