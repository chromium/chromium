// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/cors/preflight_cache.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network::cors {

namespace {

struct CacheTestEntry {
  const char* origin;
  const char* url;
};

constexpr CacheTestEntry kCacheEntries[] = {
    {"http://www.origin1.com:8080", "http://www.test.com/A"},
    {"http://www.origin2.com:80", "http://www.test.com/B"},
    {"http://www.origin3.com:80", "http://www.test.com/C"},
    {"http://www.origin4.com:80", "http://www.test.com/D"},
    {"http://A.origin.com:80", "http://www.test.com/A"},
    {"http://A.origin.com:8080", "http://www.test.com/A"},
    {"http://B.origin.com:80", "http://www.test.com/B"}};

class PreflightCacheTest : public testing::Test {
 public:
  PreflightCacheTest()
      : net_log_(
            net::NetLogWithSource::Make(net::NetLog::Get(),
                                        net::NetLogSourceType::URL_REQUEST)) {}

 protected:
  size_t CountEntries() const { return cache_.CountEntriesForTesting(); }
  void MayPurge(size_t max_entries, size_t purge_unit) {
    cache_.MayPurgeForTesting(max_entries, purge_unit);
  }
  PreflightCache* cache() { return &cache_; }

  std::unique_ptr<PreflightResult> CreateEntry() {
    return PreflightResult::Create(mojom::CredentialsMode::kInclude,
                                   std::string("POST"), std::nullopt,
                                   std::string("5"), nullptr);
  }

  void AppendEntry(const url::Origin& origin,
                   const GURL& url,
                   const net::NetworkIsolationKey& network_isolation_key,
                   mojom::IPAddressSpace target_ip_address_space =
                       mojom::IPAddressSpace::kUnknown) {
    cache_.AppendEntry(origin, url, network_isolation_key,
                       target_ip_address_space, CreateEntry());
  }

  bool CheckEntryAndRefreshCache(
      const url::Origin& origin,
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      mojom::IPAddressSpace target_ip_address_space =
          mojom::IPAddressSpace::kUnknown) {
    return cache_.CheckIfRequestCanSkipPreflight(
        origin, url, network_isolation_key, target_ip_address_space,
        network::mojom::CredentialsMode::kInclude, /*method=*/"POST",
        net::HttpRequestHeaders(), /*is_revalidating=*/false, net_log_, true);
  }

  bool CheckOptionMethodEntryAndRefreshCache(
      const url::Origin& origin,
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key) {
    return cache_.CheckIfRequestCanSkipPreflight(
        origin, url, network_isolation_key, mojom::IPAddressSpace::kUnknown,
        network::mojom::CredentialsMode::kInclude, /*method=*/"OPTION",
        net::HttpRequestHeaders(), /*is_revalidating=*/false, net_log_, true);
  }

  void ClearCache(mojom::ClearDataFilterPtr url_filter) {
    cache_.ClearCache(std::move(url_filter));
  }

  bool DoesEntryExists(const url::Origin& origin, const std::string& url) {
    return cache_.DoesEntryExistForTesting(origin, url,
                                           net::NetworkIsolationKey(),
                                           mojom::IPAddressSpace::kUnknown);
  }

  void Advance(int seconds) { clock_.Advance(base::Seconds(seconds)); }

  size_t PopulateCache() {
    for (auto entry : kCacheEntries) {
      AppendEntry(url::Origin::Create(GURL(entry.origin)), GURL(entry.url),
                  net::NetworkIsolationKey());
    }
    return std::size(kCacheEntries);
  }

 private:
  // testing::Test implementation.
  void SetUp() override { PreflightResult::SetTickClockForTesting(&clock_); }
  void TearDown() override { PreflightResult::SetTickClockForTesting(nullptr); }

  base::test::TaskEnvironment env_;
  PreflightCache cache_;
  base::SimpleTestTickClock clock_;
  net::NetLogWithSource net_log_;
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
  const std::string kOriginStr1("http://www.test.com/A");
  const url::Origin kOrigin1 = url::Origin::Create(GURL(kOriginStr1));
  const net::SchemefulSite kSite1 = net::SchemefulSite(kOrigin1);
  const net::NetworkIsolationKey kNik(kSite1, kSite1);
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
  const url::Origin kOrigin1;
  const url::Origin kOrigin2;
  const net::SchemefulSite kSite1 = net::SchemefulSite(kOrigin1);
  const net::SchemefulSite kSite2 = net::SchemefulSite(kOrigin2);
  const net::NetworkIsolationKey kNik1(kSite2, kSite1);
  const net::NetworkIsolationKey kNik2(kSite2, kSite2);
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
      kOrigin1, kUrl,
      net::NetworkIsolationKey(net::SchemefulSite(), net::SchemefulSite())));
}

TEST_F(PreflightCacheTest, PrivateNetworkAccess) {
  const url::Origin origin;
  const GURL url("http://www.test.com/A");
  const net::SchemefulSite Site = net::SchemefulSite(origin);
  const net::NetworkIsolationKey nik(Site, Site);

  // The cache starts empty.
  EXPECT_EQ(0u, CountEntries());

  AppendEntry(origin, url, nik, mojom::IPAddressSpace::kUnknown);
  EXPECT_EQ(1u, CountEntries());
  EXPECT_TRUE(CheckEntryAndRefreshCache(origin, url, nik,
                                        mojom::IPAddressSpace::kUnknown));

  AppendEntry(origin, url, nik, mojom::IPAddressSpace::kPrivate);
  AppendEntry(origin, url, nik, mojom::IPAddressSpace::kLocal);
  EXPECT_EQ(3u, CountEntries());
  EXPECT_TRUE(CheckEntryAndRefreshCache(origin, url, nik,
                                        mojom::IPAddressSpace::kPrivate));
  EXPECT_TRUE(CheckEntryAndRefreshCache(origin, url, nik,
                                        mojom::IPAddressSpace::kLocal));

  // Check that an entry we never inserted is not found in the cache.
  EXPECT_FALSE(CheckEntryAndRefreshCache(origin, url, nik,
                                         mojom::IPAddressSpace::kPublic));
}

TEST_F(PreflightCacheTest, NetLogCheckCacheExist) {
  const url::Origin kOrigin;
  const GURL kUrl("http://www.test.com/A");
  const net::SchemefulSite kSite = net::SchemefulSite(kOrigin);
  const net::NetworkIsolationKey kNik(kSite, kSite);
  net::RecordingNetLogObserver net_log_observer;

  AppendEntry(kOrigin, kUrl, kNik);

  // Cache entry's method is POST.
  EXPECT_EQ(CountEntries(), 1u);
  EXPECT_TRUE(CheckEntryAndRefreshCache(kOrigin, kUrl, kNik));
  EXPECT_FALSE(CheckOptionMethodEntryAndRefreshCache(kOrigin, kUrl, kNik));

  // Cache entry is removed once it was not sufficient to a request.
  EXPECT_EQ(CountEntries(), 0u);
  EXPECT_FALSE(CheckEntryAndRefreshCache(kOrigin, kUrl, kNik));

  AppendEntry(kOrigin, kUrl, kNik);

  // Advance time by ten seconds.
  Advance(10);

  EXPECT_EQ(CountEntries(), 1u);
  EXPECT_FALSE(CheckEntryAndRefreshCache(kOrigin, kUrl, kNik));

  std::vector<net::NetLogEntry> entries = net_log_observer.GetEntries();
  ASSERT_EQ(entries.size(), 5u);
  for (const auto& entry : entries) {
    EXPECT_EQ(entry.source.type, net::NetLogSourceType::URL_REQUEST);
  }
  EXPECT_EQ(entries[0].type, net::NetLogEventType::CHECK_CORS_PREFLIGHT_CACHE);
  EXPECT_EQ(net::GetStringValueFromParams(entries[0], "status"),
            "hit-and-pass");
  EXPECT_EQ(entries[1].type,
            net::NetLogEventType::CORS_PREFLIGHT_CACHED_RESULT);
  EXPECT_EQ(
      net::GetStringValueFromParams(entries[1], "access-control-allow-headers"),
      "");
  EXPECT_EQ(
      net::GetStringValueFromParams(entries[1], "access-control-allow-methods"),
      "POST");
  EXPECT_EQ(entries[2].type, net::NetLogEventType::CHECK_CORS_PREFLIGHT_CACHE);
  EXPECT_EQ(net::GetStringValueFromParams(entries[2], "status"),
            "hit-and-fail");
  EXPECT_EQ(entries[3].type, net::NetLogEventType::CHECK_CORS_PREFLIGHT_CACHE);
  EXPECT_EQ(net::GetStringValueFromParams(entries[3], "status"), "miss");
  EXPECT_EQ(entries[4].type, net::NetLogEventType::CHECK_CORS_PREFLIGHT_CACHE);
  EXPECT_EQ(net::GetStringValueFromParams(entries[4], "status"), "stale");
}

TEST_F(PreflightCacheTest, ClearCacheNoFilter) {
  // The cache is initially empty
  EXPECT_EQ(0u, CountEntries());
  auto numEntries = PopulateCache();
  EXPECT_EQ(numEntries, CountEntries());

  // When no filter is present, it should delete all entries in the cache
  ClearCache(nullptr /* filter */);
  EXPECT_EQ(0u, CountEntries());
}

TEST_F(PreflightCacheTest, ClearCacheWithEmptyDeleteFilter) {
  // The cache is initially empty
  EXPECT_EQ(0u, CountEntries());
  auto numEntries = PopulateCache();
  EXPECT_EQ(numEntries, CountEntries());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter::Type::DELETE_MATCHES;

  // An empty DELETE_MATCHES filter should not delete any entries
  ClearCache(std::move(filter));
  EXPECT_EQ(std::size(kCacheEntries), CountEntries());
}

TEST_F(PreflightCacheTest, ClearCacheWithDeleteFilterOrigins) {
  unsigned int filtered_entries[] = {0, 1};
  // The cache is initially empty
  EXPECT_EQ(0u, CountEntries());
  auto num_entries = PopulateCache();
  auto remaining_entries = num_entries - std::size(filtered_entries);
  EXPECT_EQ(num_entries, CountEntries());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter::Type::DELETE_MATCHES;
  for (auto i : filtered_entries) {
    filter->origins.push_back(
        url::Origin::Create(GURL(kCacheEntries[i].origin)));
  }

  // Non-empty DELETE_MATCHES should just delete the origins that match
  ClearCache(std::move(filter));
  EXPECT_EQ(remaining_entries, CountEntries());
  for (auto i : filtered_entries) {
    ASSERT_FALSE(
        DoesEntryExists(url::Origin::Create(GURL(kCacheEntries[i].origin)),
                        kCacheEntries[i].url));
  }
}

TEST_F(PreflightCacheTest, ClearCacheWithDeleteFilterDomains) {
  // The cache is initially empty
  EXPECT_EQ(0u, CountEntries());
  auto num_entries = PopulateCache();
  EXPECT_EQ(num_entries, CountEntries());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter::Type::DELETE_MATCHES;
  filter->domains.push_back("origin.com");
  // origin.com matches 3 entries for kCacheEntries
  unsigned int filtered_entries[] = {4, 5, 6};
  // Non-empty DELETE_MATCHES should just delete the origins that match
  ClearCache(std::move(filter));
  EXPECT_EQ(4u, CountEntries());
  for (auto i : filtered_entries) {
    ASSERT_FALSE(
        DoesEntryExists(url::Origin::Create(GURL(kCacheEntries[i].origin)),
                        kCacheEntries[i].url));
  }
}

TEST_F(PreflightCacheTest, ClearCacheWithDeleteFilterOriginsAndDomains) {
  // The cache is initially empty
  EXPECT_EQ(0u, CountEntries());
  auto num_entries = PopulateCache();
  EXPECT_EQ(num_entries, CountEntries());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter::Type::DELETE_MATCHES;
  unsigned int filtered_origins[] = {0, 1};
  for (auto i : filtered_origins) {
    filter->origins.push_back(
        url::Origin::Create(GURL(kCacheEntries[i].origin)));
  }
  filter->domains.push_back("origin.com");
  // Non-empty DELETE_MATCHES should just delete the origins that match
  ClearCache(std::move(filter));
  // origin.com matches 3 origins for kCacheEntries
  unsigned int matched_entries[] = {0, 1, 4, 5, 6};
  auto remaining_entries = num_entries - std::size(matched_entries);
  EXPECT_EQ(remaining_entries, CountEntries());
  for (auto i : matched_entries) {
    ASSERT_FALSE(
        DoesEntryExists(url::Origin::Create(GURL(kCacheEntries[i].origin)),
                        kCacheEntries[i].url));
  }
}

TEST_F(PreflightCacheTest, ClearCacheWithEmptyKeepFilter) {
  // The cache is initially empty
  EXPECT_EQ(0u, CountEntries());
  auto num_entries = PopulateCache();
  EXPECT_EQ(num_entries, CountEntries());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter::Type::KEEP_MATCHES;

  // An empty KEEP_MATCHES filter should delete everything
  ClearCache(std::move(filter));
  EXPECT_EQ(0u, CountEntries());
}

TEST_F(PreflightCacheTest, ClearCacheWithKeepFilterOrigins) {
  unsigned int filtered_entries[] = {3};
  // The cache is initially empty
  EXPECT_EQ(0u, CountEntries());
  auto num_entries = PopulateCache();
  EXPECT_EQ(num_entries, CountEntries());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter::Type::KEEP_MATCHES;
  for (auto i : filtered_entries) {
    filter->origins.push_back(
        url::Origin::Create(GURL(kCacheEntries[i].origin)));
  }

  // Non-empty KEEP_MATCHES should delete everything but the origins that
  // match
  auto remaining_entries = std::size(filtered_entries);
  ClearCache(std::move(filter));
  EXPECT_EQ(remaining_entries, CountEntries());
  for (auto i : filtered_entries) {
    ASSERT_TRUE(
        DoesEntryExists(url::Origin::Create(GURL(kCacheEntries[i].origin)),
                        kCacheEntries[i].url));
  }
}

TEST_F(PreflightCacheTest, ClearCacheWithKeepFilterDomains) {
  // The cache is initially empty
  EXPECT_EQ(0u, CountEntries());
  auto num_entries = PopulateCache();
  EXPECT_EQ(num_entries, CountEntries());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter::Type::KEEP_MATCHES;
  filter->domains.push_back("origin.com");
  // origin.com matches 3 entries for kCacheEntries
  unsigned int matched_entries[] = {4, 5, 6};
  // Non-empty KEEP_MATCHES should just delete the origins that match
  ClearCache(std::move(filter));
  EXPECT_EQ(std::size(matched_entries), CountEntries());
  for (auto i : matched_entries) {
    ASSERT_TRUE(
        DoesEntryExists(url::Origin::Create(GURL(kCacheEntries[i].origin)),
                        kCacheEntries[i].url));
  }
}

TEST_F(PreflightCacheTest, ClearCacheWithKeepFilterOriginsAndDomains) {
  // The cache is initially empty
  EXPECT_EQ(0u, CountEntries());
  auto num_entries = PopulateCache();
  EXPECT_EQ(num_entries, CountEntries());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter::Type::KEEP_MATCHES;
  unsigned int filtered_origins[] = {0, 1};
  for (auto i : filtered_origins) {
    filter->origins.push_back(
        url::Origin::Create(GURL(kCacheEntries[i].origin)));
  }
  filter->domains.push_back("origin.com");
  // Non-empty DELETE_MATCHES should just delete the origins that match
  ClearCache(std::move(filter));
  // origin.com matches 3 origins for kCacheEntries
  unsigned int matched_entries[] = {0, 1, 4, 5, 6};
  auto remaining_entries = std::size(matched_entries);
  EXPECT_EQ(remaining_entries, CountEntries());
  for (auto i : matched_entries) {
    ASSERT_TRUE(
        DoesEntryExists(url::Origin::Create(GURL(kCacheEntries[i].origin)),
                        kCacheEntries[i].url));
  }
}

}  // namespace

}  // namespace network::cors
