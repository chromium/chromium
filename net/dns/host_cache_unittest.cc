// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_cache.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/format_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/network_isolation_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

namespace net {

namespace {

const int kMaxCacheEntries = 10;

// Builds a key for |hostname|, defaulting the query type to unspecified.
HostCache::Key Key(const std::string& hostname) {
  return HostCache::Key(hostname, DnsQueryType::UNSPECIFIED, 0,
                        HostResolverSource::ANY, NetworkIsolationKey());
}

bool FoobarIndexIsOdd(const std::string& foobarx_com) {
  return (foobarx_com[6] - '0') % 2 == 1;
}

class MockPersistenceDelegate : public HostCache::PersistenceDelegate {
 public:
  void ScheduleWrite() override { ++num_changes_; }

  int num_changes() const { return num_changes_; }

 private:
  int num_changes_ = 0;
};

}  // namespace

TEST(HostCacheTest, Basic) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0U, cache.size());

  // Add an entry for "foobar.com" at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key1, now)->second.error() == entry.error());

  EXPECT_EQ(1U, cache.size());

  // Advance to t=5.
  now += base::TimeDelta::FromSeconds(5);

  // Add an entry for "foobar2.com" at t=5.
  EXPECT_FALSE(cache.Lookup(key2, now));
  cache.Set(key2, entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(2U, cache.size());

  // Advance to t=9
  now += base::TimeDelta::FromSeconds(4);

  // Verify that the entries we added are still retrievable, and usable.
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_NE(cache.Lookup(key1, now), cache.Lookup(key2, now));

  // Advance to t=10; key is now expired.
  now += base::TimeDelta::FromSeconds(1);

  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key2, now));

  // Update key1, so it is no longer expired.
  cache.Set(key1, entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(2U, cache.size());

  // Both entries should still be retrievable and usable.
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key2, now));

  // Advance to t=20; both entries are now expired.
  now += base::TimeDelta::FromSeconds(10);

  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
}

// Make sure NetworkIsolationKey is respected.
TEST(HostCacheTest, NetworkIsolationKey) {
  const char kHostname[] = "hostname.test";
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  const url::Origin kOrigin1(
      url::Origin::Create(GURL("https://origin1.test/")));
  const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const url::Origin kOrigin2(
      url::Origin::Create(GURL("https://origin2.test/")));
  const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  HostCache::Key key1(kHostname, DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, kNetworkIsolationKey1);
  HostCache::Key key2(kHostname, DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, kNetworkIsolationKey2);
  HostCache::Entry entry1 =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry2 = HostCache::Entry(ERR_FAILED, AddressList(),
                                             HostCache::Entry::SOURCE_UNKNOWN);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;

  EXPECT_EQ(0U, cache.size());

  // Add an entry for kNetworkIsolationKey1.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry1, now, kTTL);

  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      cache.Lookup(key1, now);
  ASSERT_TRUE(result);
  EXPECT_EQ(kNetworkIsolationKey1, result->first.network_isolation_key);
  EXPECT_EQ(OK, result->second.error());
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_EQ(1U, cache.size());

  // Add a different entry for kNetworkIsolationKey2.
  cache.Set(key2, entry2, now, 3 * kTTL);
  result = cache.Lookup(key1, now);
  ASSERT_TRUE(result);
  EXPECT_EQ(kNetworkIsolationKey1, result->first.network_isolation_key);
  EXPECT_EQ(OK, result->second.error());
  result = cache.Lookup(key2, now);
  ASSERT_TRUE(result);
  EXPECT_EQ(kNetworkIsolationKey2, result->first.network_isolation_key);
  EXPECT_EQ(ERR_FAILED, result->second.error());
  EXPECT_EQ(2U, cache.size());

  // Advance time so that first entry times out. Second entry should remain.
  now += 2 * kTTL;
  EXPECT_FALSE(cache.Lookup(key1, now));
  result = cache.Lookup(key2, now);
  ASSERT_TRUE(result);
  EXPECT_EQ(kNetworkIsolationKey2, result->first.network_isolation_key);
  EXPECT_EQ(ERR_FAILED, result->second.error());
}

// Try caching entries for a failed resolve attempt -- since we set the TTL of
// such entries to 0 it won't store, but it will kick out the previous result.
TEST(HostCacheTest, NoCacheZeroTTL) {
  const base::TimeDelta kSuccessEntryTTL = base::TimeDelta::FromSeconds(10);
  const base::TimeDelta kFailureEntryTTL = base::TimeDelta::FromSeconds(0);

  HostCache cache(kMaxCacheEntries);

  // Set t=0.
  base::TimeTicks now;

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry, now, kFailureEntryTTL);
  EXPECT_EQ(1U, cache.size());

  // We disallow use of negative entries.
  EXPECT_FALSE(cache.Lookup(key1, now));

  // Now overwrite with a valid entry, and then overwrite with negative entry
  // again -- the valid entry should be kicked out.
  cache.Set(key1, entry, now, kSuccessEntryTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  cache.Set(key1, entry, now, kFailureEntryTTL);
  EXPECT_FALSE(cache.Lookup(key1, now));
}

// Try caching entries for a failed resolves for 10 seconds.
TEST(HostCacheTest, CacheNegativeEntry) {
  const base::TimeDelta kFailureEntryTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0U, cache.size());

  // Add an entry for "foobar.com" at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry, now, kFailureEntryTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(1U, cache.size());

  // Advance to t=5.
  now += base::TimeDelta::FromSeconds(5);

  // Add an entry for "foobar2.com" at t=5.
  EXPECT_FALSE(cache.Lookup(key2, now));
  cache.Set(key2, entry, now, kFailureEntryTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(2U, cache.size());

  // Advance to t=9
  now += base::TimeDelta::FromSeconds(4);

  // Verify that the entries we added are still retrievable, and usable.
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key2, now));

  // Advance to t=10; key1 is now expired.
  now += base::TimeDelta::FromSeconds(1);

  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key2, now));

  // Update key1, so it is no longer expired.
  cache.Set(key1, entry, now, kFailureEntryTTL);
  // Re-uses existing entry storage.
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(2U, cache.size());

  // Both entries should still be retrievable and usable.
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key2, now));

  // Advance to t=20; both entries are now expired.
  now += base::TimeDelta::FromSeconds(10);

  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
}

// Tests that the same hostname can be duplicated in the cache, so long as
// the query type differs.
TEST(HostCacheTest, DnsQueryTypeIsPartOfKey) {
  const base::TimeDelta kSuccessEntryTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;

  HostCache::Key key1("foobar.com", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Key key2("foobar.com", DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkIsolationKey());
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0U, cache.size());

  // Add an entry for ("foobar.com", UNSPECIFIED) at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry, now, kSuccessEntryTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(1U, cache.size());

  // Add an entry for ("foobar.com", IPV4_ONLY) at t=0.
  EXPECT_FALSE(cache.Lookup(key2, now));
  cache.Set(key2, entry, now, kSuccessEntryTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(2U, cache.size());

  // Even though the hostnames were the same, we should have two unique
  // entries (because the address families differ).
  EXPECT_NE(cache.Lookup(key1, now), cache.Lookup(key2, now));
}

// Tests that the same hostname can be duplicated in the cache, so long as
// the HostResolverFlags differ.
TEST(HostCacheTest, HostResolverFlagsArePartOfKey) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;

  HostCache::Key key1("foobar.com", DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkIsolationKey());
  HostCache::Key key2("foobar.com", DnsQueryType::A, HOST_RESOLVER_CANONNAME,
                      HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Key key3("foobar.com", DnsQueryType::A,
                      HOST_RESOLVER_LOOPBACK_ONLY, HostResolverSource::ANY,
                      NetworkIsolationKey());
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0U, cache.size());

  // Add an entry for ("foobar.com", IPV4, NONE) at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(1U, cache.size());

  // Add an entry for ("foobar.com", IPV4, CANONNAME) at t=0.
  EXPECT_FALSE(cache.Lookup(key2, now));
  cache.Set(key2, entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(2U, cache.size());

  // Add an entry for ("foobar.com", IPV4, LOOPBACK_ONLY) at t=0.
  EXPECT_FALSE(cache.Lookup(key3, now));
  cache.Set(key3, entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key3, now));
  EXPECT_EQ(3U, cache.size());

  // Even though the hostnames were the same, we should have two unique
  // entries (because the HostResolverFlags differ).
  EXPECT_NE(cache.Lookup(key1, now), cache.Lookup(key2, now));
  EXPECT_NE(cache.Lookup(key1, now), cache.Lookup(key3, now));
  EXPECT_NE(cache.Lookup(key2, now), cache.Lookup(key3, now));
}

// Tests that the same hostname can be duplicated in the cache, so long as
// the HostResolverSource differs.
TEST(HostCacheTest, HostResolverSourceIsPartOfKey) {
  const base::TimeDelta kSuccessEntryTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;

  HostCache::Key key1("foobar.com", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Key key2("foobar.com", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::DNS, NetworkIsolationKey());
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0U, cache.size());

  // Add an entry for ("foobar.com", UNSPECIFIED, ANY) at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry, now, kSuccessEntryTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(1U, cache.size());

  // Add an entry for ("foobar.com", UNSPECIFIED, DNS) at t=0.
  EXPECT_FALSE(cache.Lookup(key2, now));
  cache.Set(key2, entry, now, kSuccessEntryTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(2U, cache.size());

  // Even though the hostnames were the same, we should have two unique
  // entries (because the HostResolverSource differs).
  EXPECT_NE(cache.Lookup(key1, now), cache.Lookup(key2, now));
}

// Tests that the same hostname can be duplicated in the cache, so long as
// the secure field in the key differs.
TEST(HostCacheTest, SecureIsPartOfKey) {
  const base::TimeDelta kSuccessEntryTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;
  HostCache::EntryStaleness stale;

  HostCache::Key key1("foobar.com", DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkIsolationKey());
  key1.secure = true;
  HostCache::Key key2("foobar.com", DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkIsolationKey());
  key2.secure = false;
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0U, cache.size());

  // Add an entry for ("foobar.com", IPV4, true /* secure */) at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry, now, kSuccessEntryTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(1U, cache.size());

  // Lookup a key that is identical to the inserted key except for the secure
  // field.
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_FALSE(cache.LookupStale(key2, now, &stale));
  const std::pair<const HostCache::Key, HostCache::Entry>* result;
  result = cache.Lookup(key2, now, true /* ignore_secure */);
  EXPECT_TRUE(result);
  EXPECT_TRUE(result->first.secure);
  result = cache.LookupStale(key2, now, &stale, true /* ignore_secure */);
  EXPECT_TRUE(result);
  EXPECT_TRUE(result->first.secure);

  // Add an entry for ("foobar.com", IPV4, false */ secure */) at t=0.
  cache.Set(key2, entry, now, kSuccessEntryTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_TRUE(cache.LookupStale(key2, now, &stale));
  EXPECT_EQ(2U, cache.size());
}

TEST(HostCacheTest, PreferLessStaleMoreSecure) {
  const base::TimeDelta kSuccessEntryTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;
  HostCache::EntryStaleness stale;

  HostCache::Key insecure_key("foobar.com", DnsQueryType::A, 0,
                              HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Key secure_key("foobar.com", DnsQueryType::A, 0,
                            HostResolverSource::ANY, NetworkIsolationKey());
  secure_key.secure = true;
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0U, cache.size());

  // Add both insecure and secure entries.
  cache.Set(insecure_key, entry, now, kSuccessEntryTTL);
  cache.Set(secure_key, entry, now, kSuccessEntryTTL);
  EXPECT_EQ(insecure_key, cache.Lookup(insecure_key, now)->first);
  EXPECT_EQ(secure_key, cache.Lookup(secure_key, now)->first);
  // Secure key is preferred when equally stale.
  EXPECT_EQ(secure_key,
            cache.Lookup(insecure_key, now, true /* ignore_secure */)->first);
  EXPECT_EQ(secure_key,
            cache.Lookup(insecure_key, now, true /* ignore_secure */)->first);

  // Simulate network change.
  cache.Invalidate();

  // Re-add insecure entry.
  cache.Set(insecure_key, entry, now, kSuccessEntryTTL);
  EXPECT_EQ(insecure_key, cache.Lookup(insecure_key, now)->first);
  EXPECT_FALSE(cache.Lookup(secure_key, now));
  EXPECT_EQ(secure_key, cache.LookupStale(secure_key, now, &stale)->first);
  // Result with fewer network changes is preferred.
  EXPECT_EQ(
      insecure_key,
      cache.LookupStale(secure_key, now, &stale, true /* ignore-secure */)
          ->first);

  // Add both insecure and secure entries to a cleared cache, still at t=0.
  cache.clear();
  cache.Set(insecure_key, entry, now, base::TimeDelta::FromSeconds(20));
  cache.Set(secure_key, entry, now, kSuccessEntryTTL);

  // Advance to t=15 to expire the secure entry only.
  now += base::TimeDelta::FromSeconds(15);
  EXPECT_EQ(insecure_key, cache.Lookup(insecure_key, now)->first);
  EXPECT_FALSE(cache.Lookup(secure_key, now));
  EXPECT_EQ(secure_key, cache.LookupStale(secure_key, now, &stale)->first);
  // Non-expired result is preferred.
  EXPECT_EQ(
      insecure_key,
      cache.LookupStale(secure_key, now, &stale, true /* ignore-secure */)
          ->first);
}

TEST(HostCacheTest, NoCache) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  // Disable caching.
  HostCache cache(0);
  EXPECT_TRUE(cache.caching_is_disabled());

  // Set t=0.
  base::TimeTicks now;

  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  // Lookup and Set should have no effect.
  EXPECT_FALSE(cache.Lookup(Key("foobar.com"), now));
  cache.Set(Key("foobar.com"), entry, now, kTTL);
  EXPECT_FALSE(cache.Lookup(Key("foobar.com"), now));

  EXPECT_EQ(0U, cache.size());
}

TEST(HostCacheTest, Clear) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // Set t=0.
  base::TimeTicks now;

  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());

  // Add three entries.
  cache.Set(Key("foobar1.com"), entry, now, kTTL);
  cache.Set(Key("foobar2.com"), entry, now, kTTL);
  cache.Set(Key("foobar3.com"), entry, now, kTTL);

  EXPECT_EQ(3u, cache.size());

  cache.clear();

  EXPECT_EQ(0u, cache.size());
}

TEST(HostCacheTest, ClearForHosts) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // Set t=0.
  base::TimeTicks now;

  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());

  // Add several entries.
  cache.Set(Key("foobar1.com"), entry, now, kTTL);
  cache.Set(Key("foobar2.com"), entry, now, kTTL);
  cache.Set(Key("foobar3.com"), entry, now, kTTL);
  cache.Set(Key("foobar4.com"), entry, now, kTTL);
  cache.Set(Key("foobar5.com"), entry, now, kTTL);

  EXPECT_EQ(5u, cache.size());

  // Clear the hosts matching a certain predicate, such as the number being odd.
  cache.ClearForHosts(base::Bind(&FoobarIndexIsOdd));

  EXPECT_EQ(2u, cache.size());
  EXPECT_TRUE(cache.Lookup(Key("foobar2.com"), now));
  EXPECT_TRUE(cache.Lookup(Key("foobar4.com"), now));

  // Passing null callback will delete all hosts.
  cache.ClearForHosts(base::Callback<bool(const std::string&)>());

  EXPECT_EQ(0u, cache.size());
}

// Try to add too many entries to cache; it should evict the one with the oldest
// expiration time.
TEST(HostCacheTest, Evict) {
  HostCache cache(2);

  base::TimeTicks now;

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Key key3 = Key("foobar3.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_FALSE(cache.Lookup(key3, now));

  // |key1| expires in 10 seconds, but |key2| in just 5.
  cache.Set(key1, entry, now, base::TimeDelta::FromSeconds(10));
  cache.Set(key2, entry, now, base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(2u, cache.size());
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_FALSE(cache.Lookup(key3, now));

  // |key2| should be chosen for eviction, since it expires sooner.
  cache.Set(key3, entry, now, base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(2u, cache.size());
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_TRUE(cache.Lookup(key3, now));
}

// Try to retrieve stale entries from the cache. They should be returned by
// |LookupStale()| but not |Lookup()|, with correct |EntryStaleness| data.
TEST(HostCacheTest, Stale) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;
  HostCache::EntryStaleness stale;

  HostCache::Key key = Key("foobar.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0U, cache.size());

  // Add an entry for "foobar.com" at t=0.
  EXPECT_FALSE(cache.Lookup(key, now));
  EXPECT_FALSE(cache.LookupStale(key, now, &stale));
  cache.Set(key, entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.LookupStale(key, now, &stale));
  EXPECT_FALSE(stale.is_stale());
  EXPECT_EQ(0, stale.stale_hits);

  EXPECT_EQ(1U, cache.size());

  // Advance to t=5.
  now += base::TimeDelta::FromSeconds(5);

  EXPECT_TRUE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.LookupStale(key, now, &stale));
  EXPECT_FALSE(stale.is_stale());
  EXPECT_EQ(0, stale.stale_hits);

  // Advance to t=15.
  now += base::TimeDelta::FromSeconds(10);

  EXPECT_FALSE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.LookupStale(key, now, &stale));
  EXPECT_TRUE(stale.is_stale());
  EXPECT_EQ(base::TimeDelta::FromSeconds(5), stale.expired_by);
  EXPECT_EQ(0, stale.network_changes);
  EXPECT_EQ(1, stale.stale_hits);

  // Advance to t=20.
  now += base::TimeDelta::FromSeconds(5);

  EXPECT_FALSE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.LookupStale(key, now, &stale));
  EXPECT_TRUE(stale.is_stale());
  EXPECT_EQ(base::TimeDelta::FromSeconds(10), stale.expired_by);
  EXPECT_EQ(0, stale.network_changes);
  EXPECT_EQ(2, stale.stale_hits);

  // Simulate network change.
  cache.Invalidate();

  EXPECT_FALSE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.LookupStale(key, now, &stale));
  EXPECT_TRUE(stale.is_stale());
  EXPECT_EQ(base::TimeDelta::FromSeconds(10), stale.expired_by);
  EXPECT_EQ(1, stale.network_changes);
  EXPECT_EQ(3, stale.stale_hits);
}

TEST(HostCacheTest, EvictStale) {
  HostCache cache(2);

  base::TimeTicks now;
  HostCache::EntryStaleness stale;

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Key key3 = Key("foobar3.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_FALSE(cache.Lookup(key3, now));

  // |key1| expires in 10 seconds.
  cache.Set(key1, entry, now, base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(1u, cache.size());
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_FALSE(cache.Lookup(key3, now));

  // Simulate network change, expiring the cache.
  cache.Invalidate();

  EXPECT_EQ(1u, cache.size());
  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.LookupStale(key1, now, &stale));
  EXPECT_EQ(1, stale.network_changes);

  // Advance to t=1.
  now += base::TimeDelta::FromSeconds(1);

  // |key2| expires before |key1| would originally have expired.
  cache.Set(key2, entry, now, base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(2u, cache.size());
  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.LookupStale(key1, now, &stale));
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_FALSE(cache.Lookup(key3, now));

  // |key1| should be chosen for eviction, since it is stale.
  cache.Set(key3, entry, now, base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(2u, cache.size());
  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.LookupStale(key1, now, &stale));
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_TRUE(cache.Lookup(key3, now));

  // Advance to t=6.
  now += base::TimeDelta::FromSeconds(5);

  // Insert |key1| again. |key3| should be evicted.
  cache.Set(key1, entry, now, base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(2u, cache.size());
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_TRUE(cache.LookupStale(key2, now, &stale));
  EXPECT_FALSE(cache.Lookup(key3, now));
  EXPECT_FALSE(cache.LookupStale(key3, now, &stale));
}

// Tests the less than and equal operators for HostCache::Key work.
TEST(HostCacheTest, KeyComparators) {
  struct CacheTestParameters {
    CacheTestParameters(const HostCache::Key key1,
                        const HostCache::Key key2,
                        int expected_comparison)
        : key1(key1), key2(key2), expected_comparison(expected_comparison) {}

    // Inputs.
    HostCache::Key key1;
    HostCache::Key key2;

    // Expectation.
    //   -1 means key1 is less than key2
    //    0 means key1 equals key2
    //    1 means key1 is greater than key2
    int expected_comparison;
  };
  std::vector<CacheTestParameters> tests = {
      {HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey()),
       HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey()),
       0},
      {HostCache::Key("host1", DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkIsolationKey()),
       HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey()),
       1},
      {HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey()),
       HostCache::Key("host1", DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkIsolationKey()),
       -1},
      {HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey()),
       HostCache::Key("host2", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey()),
       -1},
      {HostCache::Key("host1", DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkIsolationKey()),
       HostCache::Key("host2", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey()),
       1},
      {HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey()),
       HostCache::Key("host2", DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkIsolationKey()),
       -1},
      {HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey()),
       HostCache::Key("host1", DnsQueryType::UNSPECIFIED,
                      HOST_RESOLVER_CANONNAME, HostResolverSource::ANY,
                      NetworkIsolationKey()),
       -1},
      {HostCache::Key("host1", DnsQueryType::UNSPECIFIED,
                      HOST_RESOLVER_CANONNAME, HostResolverSource::ANY,
                      NetworkIsolationKey()),
       HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkIsolationKey()),
       1},
      {HostCache::Key("host1", DnsQueryType::UNSPECIFIED,
                      HOST_RESOLVER_CANONNAME, HostResolverSource::ANY,
                      NetworkIsolationKey()),
       HostCache::Key("host2", DnsQueryType::UNSPECIFIED,
                      HOST_RESOLVER_CANONNAME, HostResolverSource::ANY,
                      NetworkIsolationKey()),
       -1},
  };
  HostCache::Key insecure_key =
      HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Key secure_key =
      HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  secure_key.secure = true;
  tests.emplace_back(insecure_key, secure_key, -1);

  for (size_t i = 0; i < base::size(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]", i));

    const HostCache::Key& key1 = tests[i].key1;
    const HostCache::Key& key2 = tests[i].key2;

    switch (tests[i].expected_comparison) {
      case -1:
        EXPECT_TRUE(key1 < key2);
        EXPECT_FALSE(key2 < key1);
        break;
      case 0:
        EXPECT_FALSE(key1 < key2);
        EXPECT_FALSE(key2 < key1);
        break;
      case 1:
        EXPECT_FALSE(key1 < key2);
        EXPECT_TRUE(key2 < key1);
        break;
      default:
        FAIL() << "Invalid expectation. Can be only -1, 0, 1";
    }
  }
}

TEST(HostCacheTest, SerializeAndDeserialize) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;

  HostCache::Key key1 = Key("foobar.com");
  key1.secure = true;
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Key key3 = Key("foobar3.com");
  HostCache::Key key4 = Key("foobar4.com");

  IPAddress address_ipv4(1, 2, 3, 4);
  IPAddress address_ipv6(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  IPEndPoint endpoint_ipv4(address_ipv4, 0);
  IPEndPoint endpoint_ipv6(address_ipv6, 0);

  HostCache::Entry entry1 = HostCache::Entry(OK, AddressList(endpoint_ipv4),
                                             HostCache::Entry::SOURCE_UNKNOWN);
  AddressList addresses2 = AddressList(endpoint_ipv6);
  addresses2.push_back(endpoint_ipv4);
  HostCache::Entry entry2 =
      HostCache::Entry(OK, addresses2, HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry3 = HostCache::Entry(OK, AddressList(endpoint_ipv6),
                                             HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry4 = HostCache::Entry(OK, AddressList(endpoint_ipv4),
                                             HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());

  // Add an entry for "foobar.com" at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry1, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key1, now)->second.error() == entry1.error());

  EXPECT_EQ(1u, cache.size());

  // Advance to t=5.
  now += base::TimeDelta::FromSeconds(5);

  // Add entries for "foobar2.com" and "foobar3.com" at t=5.
  EXPECT_FALSE(cache.Lookup(key2, now));
  cache.Set(key2, entry2, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(2u, cache.size());

  EXPECT_FALSE(cache.Lookup(key3, now));
  cache.Set(key3, entry3, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key3, now));
  EXPECT_EQ(3u, cache.size());

  EXPECT_EQ(0u, cache.last_restore_size());

  // Advance to t=12, ansd serialize the cache.
  now += base::TimeDelta::FromSeconds(7);

  base::ListValue serialized_cache;
  cache.GetAsListValue(&serialized_cache, /*include_staleness=*/false);
  HostCache restored_cache(kMaxCacheEntries);

  // Add entries for "foobar3.com" and "foobar4.com" to the cache before
  // restoring it. The "foobar3.com" result is different from the original.
  EXPECT_FALSE(restored_cache.Lookup(key3, now));
  restored_cache.Set(key3, entry1, now, kTTL);
  EXPECT_TRUE(restored_cache.Lookup(key3, now));
  EXPECT_EQ(1u, restored_cache.size());

  EXPECT_FALSE(restored_cache.Lookup(key4, now));
  restored_cache.Set(key4, entry4, now, kTTL);
  EXPECT_TRUE(restored_cache.Lookup(key4, now));
  EXPECT_EQ(2u, restored_cache.size());

  EXPECT_EQ(0u, restored_cache.last_restore_size());

  restored_cache.RestoreFromListValue(serialized_cache);

  HostCache::EntryStaleness stale;

  // The "foobar.com" entry is stale due to both network changes and expiration
  // time.
  EXPECT_FALSE(restored_cache.Lookup(key1, now));
  const std::pair<const HostCache::Key, HostCache::Entry>* result1 =
      restored_cache.LookupStale(key1, now, &stale);
  EXPECT_TRUE(result1);
  EXPECT_TRUE(result1->first.secure);
  ASSERT_TRUE(result1->second.addresses());
  EXPECT_FALSE(result1->second.text_records());
  EXPECT_FALSE(result1->second.esni_data());
  EXPECT_FALSE(result1->second.hostnames());
  EXPECT_EQ(1u, result1->second.addresses().value().size());
  EXPECT_EQ(address_ipv4,
            result1->second.addresses().value().front().address());
  EXPECT_EQ(1, stale.network_changes);
  // Time to TimeTicks conversion is fuzzy, so just check that expected and
  // actual expiration times are close.
  EXPECT_GT(base::TimeDelta::FromMilliseconds(100),
            (base::TimeDelta::FromSeconds(2) - stale.expired_by).magnitude());

  // The "foobar2.com" entry is stale only due to network changes.
  EXPECT_FALSE(restored_cache.Lookup(key2, now));
  const std::pair<const HostCache::Key, HostCache::Entry>* result2 =
      restored_cache.LookupStale(key2, now, &stale);
  EXPECT_TRUE(result2);
  EXPECT_FALSE(result2->first.secure);
  ASSERT_TRUE(result2->second.addresses());
  EXPECT_EQ(2u, result2->second.addresses().value().size());
  EXPECT_EQ(address_ipv6,
            result2->second.addresses().value().front().address());
  EXPECT_EQ(address_ipv4, result2->second.addresses().value().back().address());
  EXPECT_EQ(1, stale.network_changes);
  EXPECT_GT(base::TimeDelta::FromMilliseconds(100),
            (base::TimeDelta::FromSeconds(-3) - stale.expired_by).magnitude());

  // The "foobar3.com" entry is the new one, not the restored one.
  const std::pair<const HostCache::Key, HostCache::Entry>* result3 =
      restored_cache.Lookup(key3, now);
  EXPECT_TRUE(result3);
  ASSERT_TRUE(result3->second.addresses());
  EXPECT_EQ(1u, result3->second.addresses().value().size());
  EXPECT_EQ(address_ipv4,
            result3->second.addresses().value().front().address());

  // The "foobar4.com" entry is still present and usable.
  const std::pair<const HostCache::Key, HostCache::Entry>* result4 =
      restored_cache.Lookup(key4, now);
  EXPECT_TRUE(result4);
  ASSERT_TRUE(result4->second.addresses());
  EXPECT_EQ(1u, result4->second.addresses().value().size());
  EXPECT_EQ(address_ipv4,
            result4->second.addresses().value().front().address());

  EXPECT_EQ(2u, restored_cache.last_restore_size());
}

TEST(HostCacheTest, SerializeAndDeserializeWithNetworkIsolationKey) {
  const char kHostname[] = "hostname.test";
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);
  const url::Origin kOrigin(url::Origin::Create(GURL("https://origin.test/")));
  const NetworkIsolationKey kNetworkIsolationKey(kOrigin, kOrigin);
  const url::Origin kOpaqueOrigin;
  const NetworkIsolationKey kOpaqueNetworkIsolationKey(kOpaqueOrigin,
                                                       kOpaqueOrigin);

  HostCache::Key key1(kHostname, DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, kNetworkIsolationKey);
  HostCache::Key key2(kHostname, DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, kOpaqueNetworkIsolationKey);
  IPEndPoint endpoint(IPAddress(1, 2, 3, 4), 0);

  HostCache::Entry entry = HostCache::Entry(OK, AddressList(endpoint),
                                            HostCache::Entry::SOURCE_UNKNOWN);

  base::TimeTicks now;
  HostCache cache(kMaxCacheEntries);

  cache.Set(key1, entry, now, kTTL);
  cache.Set(key2, entry, now, kTTL);

  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(kNetworkIsolationKey,
            cache.Lookup(key1, now)->first.network_isolation_key);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(kOpaqueNetworkIsolationKey,
            cache.Lookup(key2, now)->first.network_isolation_key);
  EXPECT_EQ(2u, cache.size());

  base::ListValue serialized_cache;
  cache.GetAsListValue(&serialized_cache, /*include_staleness=*/false);
  HostCache restored_cache(kMaxCacheEntries);
  EXPECT_TRUE(restored_cache.RestoreFromListValue(serialized_cache));
  EXPECT_EQ(1u, restored_cache.size());

  HostCache::EntryStaleness stale;
  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      restored_cache.LookupStale(key1, now, &stale);
  ASSERT_TRUE(result);
  EXPECT_EQ(kNetworkIsolationKey, result->first.network_isolation_key);
  EXPECT_EQ(kHostname, result->first.hostname);
  ASSERT_EQ(1u, result->second.addresses().value().size());
  EXPECT_EQ(endpoint, result->second.addresses().value().front());
  EXPECT_FALSE(restored_cache.Lookup(key2, now));
}

TEST(HostCacheTest, SerializeAndDeserialize_Text) {
  base::TimeTicks now;

  base::TimeDelta ttl = base::TimeDelta::FromSeconds(99);
  std::vector<std::string> text_records({"foo", "bar"});
  HostCache::Key key("example.com", DnsQueryType::A, 0, HostResolverSource::DNS,
                     NetworkIsolationKey());
  key.secure = true;
  HostCache::Entry entry(OK, text_records, HostCache::Entry::SOURCE_DNS, ttl);
  EXPECT_TRUE(entry.text_records());

  HostCache cache(kMaxCacheEntries);
  cache.Set(key, entry, now, ttl);
  EXPECT_EQ(1u, cache.size());

  base::ListValue serialized_cache;
  cache.GetAsListValue(&serialized_cache, false /* include_staleness */);
  HostCache restored_cache(kMaxCacheEntries);
  restored_cache.RestoreFromListValue(serialized_cache);

  ASSERT_EQ(1u, restored_cache.size());
  HostCache::EntryStaleness stale;
  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      restored_cache.LookupStale(key, now, &stale);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->first.secure);
  EXPECT_FALSE(result->second.addresses());
  ASSERT_TRUE(result->second.text_records());
  EXPECT_FALSE(result->second.hostnames());
  EXPECT_EQ(text_records, result->second.text_records().value());
}

TEST(HostCacheTest, SerializeAndDeserialize_Esni) {
  base::TimeTicks now;

  base::TimeDelta ttl = base::TimeDelta::FromSeconds(99);
  HostCache::Key key("example.com", DnsQueryType::A, 0, HostResolverSource::DNS,
                     NetworkIsolationKey());
  key.secure = true;

  const std::string kEsniKey = "a";
  const std::string kAddresslessEsniKey = "b";
  const IPAddress kAddressBack(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0);
  EsniContent esni_content;
  esni_content.AddKeyForAddress(kAddressBack, kEsniKey);
  esni_content.AddKey(kAddresslessEsniKey);
  HostCache::Entry entry(OK, esni_content, HostCache::Entry::SOURCE_DNS, ttl);
  ASSERT_TRUE(entry.esni_data());

  HostCache cache(kMaxCacheEntries);
  cache.Set(key, entry, now, ttl);
  EXPECT_EQ(1u, cache.size());

  base::ListValue serialized_cache;
  cache.GetAsListValue(&serialized_cache, false /* include_staleness */);
  HostCache restored_cache(kMaxCacheEntries);
  restored_cache.RestoreFromListValue(serialized_cache);

  ASSERT_EQ(1u, restored_cache.size());
  HostCache::EntryStaleness staleness;
  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      restored_cache.LookupStale(key, now, &staleness);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->first.secure);

  EXPECT_FALSE(result->second.addresses());
  EXPECT_FALSE(result->second.text_records());
  EXPECT_FALSE(result->second.hostnames());
  EXPECT_THAT(result->second.esni_data(), Optional(esni_content));
}

class HostCacheMalformedEsniSerializationTest : public ::testing::Test {
 public:
  HostCacheMalformedEsniSerializationTest()
      : serialized_cache_(),
        // We'll only need one entry.
        restored_cache_(1) {}

 protected:
  void SetUp() override {
    base::TimeTicks now;

    base::TimeDelta ttl = base::TimeDelta::FromSeconds(99);
    HostCache::Key key("example.com", DnsQueryType::A, 0,
                       HostResolverSource::DNS, NetworkIsolationKey());
    key.secure = true;

    const std::string esni_key = "a";
    const IPAddress kAddressBack(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                 0);
    EsniContent esni_content;
    esni_content.AddKeyForAddress(kAddressBack, esni_key);
    HostCache::Entry entry(OK, esni_content, HostCache::Entry::SOURCE_DNS, ttl);
    ASSERT_TRUE(entry.esni_data());
    HostCache cache(kMaxCacheEntries);
    cache.Set(key, entry, now, ttl);
    EXPECT_EQ(1u, cache.size());
    cache.GetAsListValue(&serialized_cache_, true /* include_staleness */);
  }

  base::ListValue serialized_cache_;
  HostCache restored_cache_;
};

// The key corresponds to kEsniDataKey from host_cache.cc.
const char kEsniDataKey[] = "esni_data";

TEST_F(HostCacheMalformedEsniSerializationTest, RejectsNonDictElement) {
  base::Value non_dict_element(base::Value::Type::LIST);

  base::Value::ListStorage cache_entries = serialized_cache_.TakeList();
  cache_entries[0].SetKey(kEsniDataKey, std::move(non_dict_element));
  serialized_cache_ = base::ListValue(std::move(cache_entries));

  EXPECT_FALSE(restored_cache_.RestoreFromListValue(serialized_cache_));
}

TEST_F(HostCacheMalformedEsniSerializationTest, RejectsNonStringAddress) {
  base::Value dict_with_non_string_value(base::Value::Type::DICTIONARY);
  dict_with_non_string_value.SetKey("a", base::Value(1));

  base::Value::ListStorage cache_entries = serialized_cache_.TakeList();
  cache_entries[0].SetKey(kEsniDataKey, std::move(dict_with_non_string_value));
  serialized_cache_ = base::ListValue(std::move(cache_entries));

  EXPECT_FALSE(restored_cache_.RestoreFromListValue(serialized_cache_));
}

TEST(HostCacheTest, SerializeAndDeserialize_Hostname) {
  base::TimeTicks now;

  base::TimeDelta ttl = base::TimeDelta::FromSeconds(99);
  std::vector<HostPortPair> hostnames(
      {HostPortPair("example.com", 95), HostPortPair("chromium.org", 122)});
  HostCache::Key key("example.com", DnsQueryType::A, 0, HostResolverSource::DNS,
                     NetworkIsolationKey());
  HostCache::Entry entry(OK, hostnames, HostCache::Entry::SOURCE_DNS, ttl);
  EXPECT_TRUE(entry.hostnames());

  HostCache cache(kMaxCacheEntries);
  cache.Set(key, entry, now, ttl);
  EXPECT_EQ(1u, cache.size());

  base::ListValue serialized_cache;
  cache.GetAsListValue(&serialized_cache, false /* include_staleness */);
  HostCache restored_cache(kMaxCacheEntries);
  restored_cache.RestoreFromListValue(serialized_cache);

  ASSERT_EQ(1u, restored_cache.size());
  HostCache::EntryStaleness stale;
  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      restored_cache.LookupStale(key, now, &stale);
  ASSERT_TRUE(result);
  EXPECT_FALSE(result->first.secure);
  EXPECT_FALSE(result->second.addresses());
  EXPECT_FALSE(result->second.text_records());
  EXPECT_FALSE(result->second.esni_data());
  ASSERT_TRUE(result->second.hostnames());
  EXPECT_EQ(hostnames, result->second.hostnames().value());
}

TEST(HostCacheTest, PersistenceDelegate) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);
  HostCache cache(kMaxCacheEntries);
  MockPersistenceDelegate delegate;
  cache.set_persistence_delegate(&delegate);

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");

  IPAddress address_ipv4(1, 2, 3, 4);
  IPAddress address_ipv6(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  IPEndPoint endpoint_ipv4(address_ipv4, 0);
  IPEndPoint endpoint_ipv6(address_ipv6, 0);

  HostCache::Entry entry1 = HostCache::Entry(OK, AddressList(endpoint_ipv4),
                                             HostCache::Entry::SOURCE_UNKNOWN);
  AddressList addresses2 = AddressList(endpoint_ipv6);
  addresses2.push_back(endpoint_ipv4);
  HostCache::Entry entry2 =
      HostCache::Entry(OK, addresses2, HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry3 = HostCache::Entry(
      ERR_NAME_NOT_RESOLVED, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry4 =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN);

  // Start at t=0.
  base::TimeTicks now;
  EXPECT_EQ(0u, cache.size());

  // Add two entries at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry1, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(1, delegate.num_changes());

  EXPECT_FALSE(cache.Lookup(key2, now));
  cache.Set(key2, entry3, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(2, delegate.num_changes());

  // Advance to t=5.
  now += base::TimeDelta::FromSeconds(5);

  // Changes that shouldn't trigger a write:
  // Add an entry for "foobar.com" with different expiration time.
  EXPECT_TRUE(cache.Lookup(key1, now));
  cache.Set(key1, entry1, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(2, delegate.num_changes());

  // Add an entry for "foobar.com" with different TTL.
  EXPECT_TRUE(cache.Lookup(key1, now));
  cache.Set(key1, entry1, now, kTTL - base::TimeDelta::FromSeconds(5));
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(2, delegate.num_changes());

  // Changes that should trigger a write:
  // Add an entry for "foobar.com" with different address list.
  EXPECT_TRUE(cache.Lookup(key1, now));
  cache.Set(key1, entry2, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(3, delegate.num_changes());

  // Add an entry for "foobar2.com" with different error.
  EXPECT_TRUE(cache.Lookup(key1, now));
  cache.Set(key2, entry4, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(4, delegate.num_changes());
}

TEST(HostCacheTest, MergeEntries) {
  const IPAddress kAddressFront(1, 2, 3, 4);
  const IPEndPoint kEndpointFront(kAddressFront, 0);
  HostCache::Entry front(OK, AddressList(kEndpointFront),
                         HostCache::Entry::SOURCE_DNS);
  front.set_text_records(std::vector<std::string>{"text1"});
  const HostPortPair kHostnameFront("host", 1);
  front.set_hostnames(std::vector<HostPortPair>{kHostnameFront});

  const IPAddress kAddressBack(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0);
  const IPEndPoint kEndpointBack(kAddressBack, 0);
  HostCache::Entry back(OK, AddressList(kEndpointBack),
                        HostCache::Entry::SOURCE_DNS);
  back.set_text_records(std::vector<std::string>{"text2"});
  const HostPortPair kHostnameBack("host", 2);
  back.set_hostnames(std::vector<HostPortPair>{kHostnameBack});

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  // Expect the IPv6 address to precede the IPv4 address.
  EXPECT_THAT(result.addresses(),
              Optional(Property(&AddressList::endpoints,
                                ElementsAre(kEndpointBack, kEndpointFront))));
  EXPECT_THAT(result.text_records(), Optional(ElementsAre("text1", "text2")));

  EXPECT_THAT(result.hostnames(),
              Optional(ElementsAre(kHostnameFront, kHostnameBack)));
}

IPAddress MakeIP(base::StringPiece literal) {
  IPAddress ret;
  CHECK(ret.AssignFromIPLiteral(literal));
  return ret;
}

IPAddressList MakeIPList(std::vector<std::string> my_addresses) {
  IPAddressList out(my_addresses.size());
  std::transform(my_addresses.begin(), my_addresses.end(), out.begin(),
                 &MakeIP);
  return out;
}

std::vector<IPEndPoint> MakeEndpoints(std::vector<std::string> my_addresses) {
  std::vector<IPEndPoint> out(my_addresses.size());
  std::transform(my_addresses.begin(), my_addresses.end(), out.begin(),
                 [](auto& s) { return IPEndPoint(MakeIP(s), 0); });
  return out;
}

TEST(HostCacheTest, SortsAndDeduplicatesAddresses) {
  IPAddressList front_addresses = MakeIPList({"0.0.0.1", "0.0.0.1", "0.0.0.2"});
  IPAddressList back_addresses =
      MakeIPList({"0.0.0.2", "0.0.0.2", "::3", "::3"});

  HostCache::Entry front(
      OK, AddressList::CreateFromIPAddressList(front_addresses, "front"),
      HostCache::Entry::SOURCE_DNS);
  HostCache::Entry back(
      OK, AddressList::CreateFromIPAddressList(back_addresses, "back"),
      HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  EXPECT_THAT(
      result.addresses(),
      Optional(Property(
          &AddressList::endpoints,
          ElementsAreArray(MakeEndpoints({"::3", "0.0.0.1", "0.0.0.2"})))));
}

TEST(HostCacheTest, PrefersAddressesWithEsniContent) {
  IPAddressList front_addresses = MakeIPList({"0.0.0.2", "0.0.0.4"});
  IPAddressList back_addresses =
      MakeIPList({"0.0.0.2", "0.0.0.2", "::3", "::3", "0.0.0.4"});

  EsniContent esni_content_front, esni_content_back;
  esni_content_front.AddKeyForAddress(MakeIP("0.0.0.4"), "key for 0.0.0.4");
  esni_content_back.AddKeyForAddress(MakeIP("::3"), "key for ::3");

  HostCache::Entry front(
      OK, AddressList::CreateFromIPAddressList(front_addresses, "front"),
      HostCache::Entry::SOURCE_DNS);
  front.set_esni_data(esni_content_front);
  HostCache::Entry back(
      OK, AddressList::CreateFromIPAddressList(back_addresses, "back"),
      HostCache::Entry::SOURCE_DNS);
  back.set_esni_data(esni_content_back);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_THAT(
      result.addresses(),
      Optional(Property(
          &AddressList::endpoints,
          ElementsAreArray(MakeEndpoints({"::3", "0.0.0.4", "0.0.0.2"})))));

  EXPECT_THAT(result.esni_data(),
              Optional(Property(
                  &EsniContent::keys_for_addresses,
                  UnorderedElementsAre(
                      Pair(MakeIP("::3"), UnorderedElementsAre("key for ::3")),
                      Pair(MakeIP("0.0.0.4"),
                           UnorderedElementsAre("key for 0.0.0.4"))))));
}

TEST(HostCacheTest, MergesManyEntriesWithEsniContent) {
  IPAddressList front_addresses, back_addresses;
  EsniContent esni_content_front, esni_content_back;

  // Add several IPv4 and IPv6 addresses to both the front and
  // back ESNI structs and address_lists, and associate some of each
  // with ESNI keys.
  const std::string ipv4_prefix = "1.2.3.", ipv6_prefix = "::";
  for (int i = 0; i < 50; ++i) {
    IPAddress next =
        MakeIP((i % 2 ? ipv4_prefix : ipv6_prefix) + base::NumberToString(i));
    bool is_front = !!(i % 3);
    if (is_front) {
      front_addresses.push_back(next);
    } else {
      back_addresses.push_back(next);
    }
    if (i % 5) {
      std::string key = base::NumberToString(i % 5);
      if (is_front) {
        esni_content_front.AddKeyForAddress(next, key);
      } else {
        esni_content_back.AddKeyForAddress(next, key);
      }
    }
  }

  HostCache::Entry front(
      OK,
      AddressList::CreateFromIPAddressList(front_addresses, "front_canonname"),
      HostCache::Entry::SOURCE_DNS);
  front.set_esni_data(esni_content_front);

  HostCache::Entry back(
      OK,
      AddressList::CreateFromIPAddressList(back_addresses, "back_canonname"),
      HostCache::Entry::SOURCE_DNS);
  back.set_esni_data(esni_content_back);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  ASSERT_TRUE(result.addresses());
  EXPECT_EQ(result.addresses()->canonical_name(), "front_canonname");

  EXPECT_EQ(result.addresses()->size(),
            std::set<IPEndPoint>(result.addresses()->begin(),
                                 result.addresses()->end())
                .size())
      << "Addresses should have been deduplicated.";

  ASSERT_TRUE(result.esni_data());

  auto has_keys = [&](const IPEndPoint& e) {
    return !!result.esni_data()->keys_for_addresses().count(e.address());
  };

  // Helper for determining whether the resulting addresses are correctly
  // ordered. Returns true if it's an error for |e2| to come before |e1| in
  // *results.addresses().
  auto address_must_precede = [&](const IPEndPoint& e1,
                                  const IPEndPoint& e2) -> bool {
    if (has_keys(e1) != has_keys(e2)) {
      return has_keys(e1) && !has_keys(e2);
    }
    if (e1.address().IsIPv6() != e2.address().IsIPv6()) {
      return e1.address().IsIPv6() && !e2.address().IsIPv6();
    }

    // If e1 and e2 were in the same input entry, and they're otherwise
    // tied in the precedence ordering, then their order in the input entry
    // should be preserved in the output.
    bool e1_in_front = base::Contains(front_addresses, e1.address());
    bool e2_in_front = base::Contains(front_addresses, e2.address());
    bool e1_in_back = base::Contains(back_addresses, e1.address());
    bool e2_in_back = base::Contains(back_addresses, e2.address());
    if (e1_in_front == e2_in_front && e1_in_front != e1_in_back &&
        e2_in_front != e2_in_back) {
      const IPAddressList common_list =
          e1_in_front ? front_addresses : back_addresses;
      return std::find(common_list.begin(), common_list.end(), e1.address()) <
             std::find(common_list.begin(), common_list.end(), e2.address());
    }
    return false;
  };

  for (size_t i = 0; i < result.addresses()->size() - 1; ++i) {
    EXPECT_FALSE(address_must_precede((*result.addresses())[i + 1],
                                      (*result.addresses())[i]));
  }

  auto esni_content_merged = esni_content_front;
  esni_content_merged.MergeFrom(esni_content_back);
  EXPECT_THAT(result.esni_data(), Optional(esni_content_merged));
}

TEST(HostCacheTest, MergeEntries_frontEmpty) {
  HostCache::Entry front(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);

  const IPAddress kAddressBack(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0);
  const IPEndPoint kEndpointBack(kAddressBack, 0);
  HostCache::Entry back(OK, AddressList(kEndpointBack),
                        HostCache::Entry::SOURCE_DNS,
                        base::TimeDelta::FromHours(4));
  back.set_text_records(std::vector<std::string>{"text2"});
  EsniContent esni_content_back;
  const std::string esni_key = "a";
  esni_content_back.AddKeyForAddress(kAddressBack, esni_key);
  back.set_esni_data(esni_content_back);
  const HostPortPair kHostnameBack("host", 2);
  back.set_hostnames(std::vector<HostPortPair>{kHostnameBack});

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  ASSERT_TRUE(result.addresses());
  EXPECT_THAT(result.addresses().value().endpoints(),
              ElementsAre(kEndpointBack));
  EXPECT_THAT(result.text_records(), Optional(ElementsAre("text2")));
  EXPECT_THAT(result.hostnames(), Optional(ElementsAre(kHostnameBack)));
  EXPECT_THAT(result.esni_data(), Optional(esni_content_back));

  EXPECT_EQ(base::TimeDelta::FromHours(4), result.ttl());
}

TEST(HostCacheTest, MergeEntries_backEmpty) {
  const IPAddress kAddressFront(1, 2, 3, 4);
  const IPEndPoint kEndpointFront(kAddressFront, 0);
  HostCache::Entry front(OK, AddressList(kEndpointFront),
                         HostCache::Entry::SOURCE_DNS,
                         base::TimeDelta::FromMinutes(5));
  front.set_text_records(std::vector<std::string>{"text1"});
  EsniContent esni_content_front;
  const std::string esni_key = "a";
  esni_content_front.AddKeyForAddress(kAddressFront, esni_key);
  front.set_esni_data(esni_content_front);
  const HostPortPair kHostnameFront("host", 1);
  front.set_hostnames(std::vector<HostPortPair>{kHostnameFront});

  HostCache::Entry back(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  ASSERT_TRUE(result.addresses());
  EXPECT_THAT(result.addresses().value().endpoints(),
              ElementsAre(kEndpointFront));
  EXPECT_THAT(result.text_records(), Optional(ElementsAre("text1")));
  EXPECT_THAT(result.hostnames(), Optional(ElementsAre(kHostnameFront)));
  EXPECT_THAT(result.esni_data(), Optional(esni_content_front));

  EXPECT_EQ(base::TimeDelta::FromMinutes(5), result.ttl());
}

TEST(HostCacheTest, MergeEntries_bothEmpty) {
  HostCache::Entry front(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);
  HostCache::Entry back(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  EXPECT_FALSE(result.addresses());
  EXPECT_FALSE(result.text_records());
  EXPECT_FALSE(result.hostnames());
  EXPECT_FALSE(result.esni_data());
  EXPECT_FALSE(result.has_ttl());
}

TEST(HostCacheTest, MergeEntries_differentTtl) {
  HostCache::Entry front(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS,
                         base::TimeDelta::FromDays(12));
  HostCache::Entry back(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS,
                        base::TimeDelta::FromSeconds(42));

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(base::TimeDelta::FromSeconds(42), result.ttl());
}

TEST(HostCacheTest, MergeEntries_FrontCannonnamePreserved) {
  AddressList addresses_front;
  const std::string kCanonicalNameFront = "name1";
  addresses_front.set_canonical_name(kCanonicalNameFront);
  HostCache::Entry front(OK, addresses_front, HostCache::Entry::SOURCE_DNS);

  AddressList addresses_back;
  const std::string kCanonicalNameBack = "name2";
  addresses_back.set_canonical_name(kCanonicalNameBack);
  HostCache::Entry back(OK, addresses_back, HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  ASSERT_TRUE(result.addresses());
  EXPECT_EQ(kCanonicalNameFront, result.addresses().value().canonical_name());
}

// Test that the back canonname can be used if there is no front cannonname.
TEST(HostCacheTest, MergeEntries_BackCannonnameUsable) {
  AddressList addresses_front;
  const std::string kCanonicalNameFront = "";
  addresses_front.set_canonical_name(kCanonicalNameFront);
  HostCache::Entry front(OK, addresses_front, HostCache::Entry::SOURCE_DNS);

  AddressList addresses_back;
  const std::string kCanonicalNameBack = "name2";
  addresses_back.set_canonical_name(kCanonicalNameBack);
  HostCache::Entry back(OK, addresses_back, HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  ASSERT_TRUE(result.addresses());
  EXPECT_EQ(kCanonicalNameBack, result.addresses().value().canonical_name());
}

void GetMatchingKeyHelper(const HostCache::Key key, bool expect_match) {
  HostCache cache(kMaxCacheEntries);
  HostCache::Entry entry =
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_DNS);

  // t=0.
  base::TimeTicks now;
  HostCache::Entry::Source source;
  HostCache::EntryStaleness stale;

  cache.Set(key, entry, now, base::TimeDelta::FromSeconds(10));

  const HostCache::Key* result =
      cache.GetMatchingKey(key.hostname, &source, &stale);
  EXPECT_EQ(expect_match, (result != nullptr));
  if (result) {
    EXPECT_EQ(key.hostname, result->hostname);
    EXPECT_EQ(key.secure, result->secure);
    EXPECT_EQ(key.dns_query_type, result->dns_query_type);
    EXPECT_EQ(key.host_resolver_flags, result->host_resolver_flags);
    EXPECT_EQ(HostCache::Entry::SOURCE_DNS, source);
  }
}

TEST(HostCacheTest, GetMatchingKey_ExactMatch) {
  // Should find match because this mimics the default Key struct.
  GetMatchingKeyHelper(
      HostCache::Key("foobar.com", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey()),
      true);
}

TEST(HostCacheTest, GetMatchingKey_IgnoreSecureField) {
  // Should find match because lookups ignore the secure field.
  HostCache::Key secure_key =
      HostCache::Key("foobar.com", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  secure_key.secure = true;
  GetMatchingKeyHelper(secure_key, true);
}

TEST(HostCacheTest, GetMatchingKey_UnsupportedDnsQueryType) {
  // Should not find match because the DnsQueryType field matters.
  GetMatchingKeyHelper(
      HostCache::Key("foobar.com", DnsQueryType::A, 0, HostResolverSource::ANY,
                     NetworkIsolationKey()),
      false);
}

TEST(HostCacheTest, GetMatchingKey_UnsupportedHostResolverFlags) {
  // Should not find match because the HostResolverFlags field matters.
  GetMatchingKeyHelper(
      HostCache::Key("foobar.com", DnsQueryType::UNSPECIFIED,
                     HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6,
                     HostResolverSource::ANY, NetworkIsolationKey()),
      false);
}

TEST(HostCacheTest, GetMatchingKey_UnsupportedHostResolverSource) {
  // Should not find match because the HostResolverSource field matters.
  GetMatchingKeyHelper(
      HostCache::Key("foobar.com", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::DNS, NetworkIsolationKey()),
      false);
}

TEST(HostCacheTest, GetMatchingKey_AlternativeMatch) {
  // Should find match because a lookup with these alternate fields is tried.
  HostCache::Key secure_key =
      HostCache::Key("foobar.com", DnsQueryType::A,
                     HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6,
                     HostResolverSource::ANY, NetworkIsolationKey());
  secure_key.secure = true;
  GetMatchingKeyHelper(secure_key, true);
}

}  // namespace net
