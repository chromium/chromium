// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_cache.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/host_resolver_results_test_util.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/host_resolver_results.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

namespace net {

namespace {

const int kMaxCacheEntries = 10;

// Builds a key for |hostname|, defaulting the query type to unspecified.
HostCache::Key Key(const std::string& hostname) {
  return HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, hostname, 443),
                        DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                        NetworkAnonymizationKey());
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

MATCHER_P(EntryContentsEqual,
          entry,
          base::StrCat({"contents ", negation ? "!=" : "==", " contents of ",
                        testing::PrintToString(entry)})) {
  return arg.ContentsEqual(entry);
}

IPAddress MakeIP(std::string_view literal) {
  IPAddress ret;
  CHECK(ret.AssignFromIPLiteral(literal));
  return ret;
}

std::vector<IPEndPoint> MakeEndpoints(std::vector<std::string> my_addresses) {
  std::vector<IPEndPoint> out(my_addresses.size());
  base::ranges::transform(my_addresses, out.begin(),
                          [](auto& s) { return IPEndPoint(MakeIP(s), 0); });
  return out;
}

}  // namespace

TEST(HostCacheTest, Basic) {
  const base::TimeDelta kTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{"foobar.com"},
                       HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0U, cache.size());

  // Add an entry for "foobar.com" at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key1, now)->second.error() == entry.error());

  EXPECT_EQ(1U, cache.size());

  // Advance to t=5.
  now += base::Seconds(5);

  // Add an entry for "foobar2.com" at t=5.
  EXPECT_FALSE(cache.Lookup(key2, now));
  cache.Set(key2, entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(2U, cache.size());

  // Advance to t=9
  now += base::Seconds(4);

  // Verify that the entries we added are still retrievable, and usable.
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_NE(cache.Lookup(key1, now), cache.Lookup(key2, now));

  // Advance to t=10; key is now expired.
  now += base::Seconds(1);

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
  now += base::Seconds(10);

  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
}

TEST(HostCacheTest, GetEndpoints) {
  std::vector<IPEndPoint> ip_endpoints = {IPEndPoint(IPAddress(1, 1, 1, 1), 0),
                                          IPEndPoint(IPAddress(2, 2, 2, 2), 0)};
  HostCache::Entry entry(OK, ip_endpoints, /*aliases=*/{},
                         HostCache::Entry::SOURCE_DNS);

  EXPECT_THAT(entry.GetEndpoints(),
              ElementsAre(ExpectEndpointResult(ip_endpoints)));
}

TEST(HostCacheTest, GetEmptyEndpoints) {
  HostCache::Entry entry(ERR_NAME_NOT_RESOLVED, /*ip_endpoints=*/{},
                         /*aliases=*/{}, HostCache::Entry::SOURCE_DNS);
  EXPECT_THAT(entry.GetEndpoints(), IsEmpty());
}

TEST(HostCacheTest, GetEmptyEndpointsWithMetadata) {
  HostCache::Entry entry(ERR_NAME_NOT_RESOLVED, /*ip_endpoints=*/{},
                         /*aliases=*/{}, HostCache::Entry::SOURCE_DNS);

  // Merge in non-empty metadata.
  ConnectionEndpointMetadata metadata;
  metadata.supported_protocol_alpns = {"h3", "h2"};
  HostCache::Entry metadata_entry(
      OK,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{
          {1u, metadata}},
      HostCache::Entry::SOURCE_DNS);

  auto merged_entry = HostCache::Entry::MergeEntries(entry, metadata_entry);

  // Result should still be empty.
  EXPECT_THAT(merged_entry.GetEndpoints(), IsEmpty());
}

TEST(HostCacheTest, GetMissingEndpoints) {
  HostCache::Entry entry(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);
  EXPECT_THAT(entry.GetEndpoints(), IsEmpty());
}

TEST(HostCacheTest, GetMissingEndpointsWithMetadata) {
  HostCache::Entry entry(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);

  // Merge in non-empty metadata.
  ConnectionEndpointMetadata metadata;
  metadata.supported_protocol_alpns = {"h3", "h2"};
  HostCache::Entry metadata_entry(
      OK,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{
          {1u, metadata}},
      HostCache::Entry::SOURCE_DNS);

  auto merged_entry = HostCache::Entry::MergeEntries(entry, metadata_entry);

  // Result should still be empty.
  EXPECT_THAT(merged_entry.GetEndpoints(), IsEmpty());
}

// Test that Keys without scheme are allowed and treated as completely different
// from similar Keys with scheme.
TEST(HostCacheTest, HandlesKeysWithoutScheme) {
  const base::TimeDelta kTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;

  HostCache::Key key("host1.test", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkAnonymizationKey());
  HostCache::Key key_with_scheme(
      url::SchemeHostPort(url::kHttpsScheme, "host1.test", 443),
      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
      NetworkAnonymizationKey());
  ASSERT_NE(key, key_with_scheme);
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

  ASSERT_EQ(0U, cache.size());
  ASSERT_FALSE(cache.Lookup(key, now));
  ASSERT_FALSE(cache.Lookup(key_with_scheme, now));

  // Add entry for `key`.
  cache.Set(key, entry, now, kTTL);
  EXPECT_EQ(1U, cache.size());
  EXPECT_TRUE(cache.Lookup(key, now));
  EXPECT_FALSE(cache.Lookup(key_with_scheme, now));

  // Add entry for `key_with_scheme`.
  cache.Set(key_with_scheme, entry, now, kTTL);
  EXPECT_EQ(2U, cache.size());
  EXPECT_TRUE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.Lookup(key_with_scheme, now));

  // Clear the cache and try adding in reverse order.
  cache.clear();
  ASSERT_EQ(0U, cache.size());
  ASSERT_FALSE(cache.Lookup(key, now));
  ASSERT_FALSE(cache.Lookup(key_with_scheme, now));

  // Add entry for `key_with_scheme`.
  cache.Set(key_with_scheme, entry, now, kTTL);
  EXPECT_EQ(1U, cache.size());
  EXPECT_FALSE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.Lookup(key_with_scheme, now));

  // Add entry for `key`.
  cache.Set(key, entry, now, kTTL);
  EXPECT_EQ(2U, cache.size());
  EXPECT_TRUE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.Lookup(key_with_scheme, now));
}

// Make sure NetworkAnonymizationKey is respected.
TEST(HostCacheTest, NetworkAnonymizationKey) {
  const url::SchemeHostPort kHost(url::kHttpsScheme, "hostname.test", 443);
  const base::TimeDelta kTTL = base::Seconds(10);

  const SchemefulSite kSite1(GURL("https://site1.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://site2.test/"));
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  HostCache::Key key1(kHost, DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, kNetworkAnonymizationKey1);
  HostCache::Key key2(kHost, DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, kNetworkAnonymizationKey2);
  HostCache::Entry entry1 =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry2 =
      HostCache::Entry(ERR_FAILED, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;

  EXPECT_EQ(0U, cache.size());

  // Add an entry for kNetworkAnonymizationKey1.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry1, now, kTTL);

  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      cache.Lookup(key1, now);
  ASSERT_TRUE(result);
  EXPECT_EQ(kNetworkAnonymizationKey1, result->first.network_anonymization_key);
  EXPECT_EQ(OK, result->second.error());
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_EQ(1U, cache.size());

  // Add a different entry for kNetworkAnonymizationKey2.
  cache.Set(key2, entry2, now, 3 * kTTL);
  result = cache.Lookup(key1, now);
  ASSERT_TRUE(result);
  EXPECT_EQ(kNetworkAnonymizationKey1, result->first.network_anonymization_key);
  EXPECT_EQ(OK, result->second.error());
  result = cache.Lookup(key2, now);
  ASSERT_TRUE(result);
  EXPECT_EQ(kNetworkAnonymizationKey2, result->first.network_anonymization_key);
  EXPECT_EQ(ERR_FAILED, result->second.error());
  EXPECT_EQ(2U, cache.size());

  // Advance time so that first entry times out. Second entry should remain.
  now += 2 * kTTL;
  EXPECT_FALSE(cache.Lookup(key1, now));
  result = cache.Lookup(key2, now);
  ASSERT_TRUE(result);
  EXPECT_EQ(kNetworkAnonymizationKey2, result->first.network_anonymization_key);
  EXPECT_EQ(ERR_FAILED, result->second.error());
}

// Try caching entries for a failed resolve attempt -- since we set the TTL of
// such entries to 0 it won't store, but it will kick out the previous result.
TEST(HostCacheTest, NoCacheZeroTTL) {
  const base::TimeDelta kSuccessEntryTTL = base::Seconds(10);
  const base::TimeDelta kFailureEntryTTL = base::Seconds(0);

  HostCache cache(kMaxCacheEntries);

  // Set t=0.
  base::TimeTicks now;

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

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
  const base::TimeDelta kFailureEntryTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0U, cache.size());

  // Add an entry for "foobar.com" at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry, now, kFailureEntryTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(1U, cache.size());

  // Advance to t=5.
  now += base::Seconds(5);

  // Add an entry for "foobar2.com" at t=5.
  EXPECT_FALSE(cache.Lookup(key2, now));
  cache.Set(key2, entry, now, kFailureEntryTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(2U, cache.size());

  // Advance to t=9
  now += base::Seconds(4);

  // Verify that the entries we added are still retrievable, and usable.
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key2, now));

  // Advance to t=10; key1 is now expired.
  now += base::Seconds(1);

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
  now += base::Seconds(10);

  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
}

// Tests that the same hostname can be duplicated in the cache, so long as
// the query type differs.
TEST(HostCacheTest, DnsQueryTypeIsPartOfKey) {
  const base::TimeDelta kSuccessEntryTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;

  HostCache::Key key1(url::SchemeHostPort(url::kHttpScheme, "foobar.com", 80),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey());
  HostCache::Key key2(url::SchemeHostPort(url::kHttpScheme, "foobar.com", 80),
                      DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey());
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

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
  const url::SchemeHostPort kHost(url::kHttpsScheme, "foobar.test", 443);
  const base::TimeDelta kTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;

  HostCache::Key key1(kHost, DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey());
  HostCache::Key key2(kHost, DnsQueryType::A, HOST_RESOLVER_CANONNAME,
                      HostResolverSource::ANY, NetworkAnonymizationKey());
  HostCache::Key key3(kHost, DnsQueryType::A, HOST_RESOLVER_LOOPBACK_ONLY,
                      HostResolverSource::ANY, NetworkAnonymizationKey());
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

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
  const url::SchemeHostPort kHost(url::kHttpsScheme, "foobar.test", 443);
  const base::TimeDelta kSuccessEntryTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;

  HostCache::Key key1(kHost, DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkAnonymizationKey());
  HostCache::Key key2(kHost, DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::DNS, NetworkAnonymizationKey());
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

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
  const url::SchemeHostPort kHost(url::kHttpsScheme, "foobar.test", 443);
  const base::TimeDelta kSuccessEntryTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;
  HostCache::EntryStaleness stale;

  HostCache::Key key1(kHost, DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey());
  key1.secure = true;
  HostCache::Key key2(kHost, DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey());
  key2.secure = false;
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

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
  const url::SchemeHostPort kHost(url::kHttpsScheme, "foobar.test", 443);
  const base::TimeDelta kSuccessEntryTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // t=0.
  base::TimeTicks now;
  HostCache::EntryStaleness stale;

  HostCache::Key insecure_key(kHost, DnsQueryType::A, 0,
                              HostResolverSource::ANY,
                              NetworkAnonymizationKey());
  HostCache::Key secure_key(kHost, DnsQueryType::A, 0, HostResolverSource::ANY,
                            NetworkAnonymizationKey());
  secure_key.secure = true;
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

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
  cache.Set(insecure_key, entry, now, base::Seconds(20));
  cache.Set(secure_key, entry, now, kSuccessEntryTTL);

  // Advance to t=15 to expire the secure entry only.
  now += base::Seconds(15);
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
  const base::TimeDelta kTTL = base::Seconds(10);

  // Disable caching.
  HostCache cache(0);
  EXPECT_TRUE(cache.caching_is_disabled());

  // Set t=0.
  base::TimeTicks now;

  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

  // Lookup and Set should have no effect.
  EXPECT_FALSE(cache.Lookup(Key("foobar.com"), now));
  cache.Set(Key("foobar.com"), entry, now, kTTL);
  EXPECT_FALSE(cache.Lookup(Key("foobar.com"), now));

  EXPECT_EQ(0U, cache.size());
}

TEST(HostCacheTest, Clear) {
  const base::TimeDelta kTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // Set t=0.
  base::TimeTicks now;

  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

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
  const base::TimeDelta kTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // Set t=0.
  base::TimeTicks now;

  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());

  // Add several entries.
  cache.Set(Key("foobar1.com"), entry, now, kTTL);
  cache.Set(Key("foobar2.com"), entry, now, kTTL);
  cache.Set(Key("foobar3.com"), entry, now, kTTL);
  cache.Set(Key("foobar4.com"), entry, now, kTTL);
  cache.Set(Key("foobar5.com"), entry, now, kTTL);

  EXPECT_EQ(5u, cache.size());

  // Clear the hosts matching a certain predicate, such as the number being odd.
  cache.ClearForHosts(base::BindRepeating(&FoobarIndexIsOdd));

  EXPECT_EQ(2u, cache.size());
  EXPECT_TRUE(cache.Lookup(Key("foobar2.com"), now));
  EXPECT_TRUE(cache.Lookup(Key("foobar4.com"), now));

  // Passing null callback will delete all hosts.
  cache.ClearForHosts(base::NullCallback());

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
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_FALSE(cache.Lookup(key3, now));

  // |key1| expires in 10 seconds, but |key2| in just 5.
  cache.Set(key1, entry, now, base::Seconds(10));
  cache.Set(key2, entry, now, base::Seconds(5));
  EXPECT_EQ(2u, cache.size());
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_FALSE(cache.Lookup(key3, now));

  // |key2| should be chosen for eviction, since it expires sooner.
  cache.Set(key3, entry, now, base::Seconds(10));
  EXPECT_EQ(2u, cache.size());
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_TRUE(cache.Lookup(key3, now));
}

// Try to retrieve stale entries from the cache. They should be returned by
// |LookupStale()| but not |Lookup()|, with correct |EntryStaleness| data.
TEST(HostCacheTest, Stale) {
  const base::TimeDelta kTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;
  HostCache::EntryStaleness stale;

  HostCache::Key key = Key("foobar.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

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
  now += base::Seconds(5);

  EXPECT_TRUE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.LookupStale(key, now, &stale));
  EXPECT_FALSE(stale.is_stale());
  EXPECT_EQ(0, stale.stale_hits);

  // Advance to t=15.
  now += base::Seconds(10);

  EXPECT_FALSE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.LookupStale(key, now, &stale));
  EXPECT_TRUE(stale.is_stale());
  EXPECT_EQ(base::Seconds(5), stale.expired_by);
  EXPECT_EQ(0, stale.network_changes);
  EXPECT_EQ(1, stale.stale_hits);

  // Advance to t=20.
  now += base::Seconds(5);

  EXPECT_FALSE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.LookupStale(key, now, &stale));
  EXPECT_TRUE(stale.is_stale());
  EXPECT_EQ(base::Seconds(10), stale.expired_by);
  EXPECT_EQ(0, stale.network_changes);
  EXPECT_EQ(2, stale.stale_hits);

  // Simulate network change.
  cache.Invalidate();

  EXPECT_FALSE(cache.Lookup(key, now));
  EXPECT_TRUE(cache.LookupStale(key, now, &stale));
  EXPECT_TRUE(stale.is_stale());
  EXPECT_EQ(base::Seconds(10), stale.expired_by);
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
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_FALSE(cache.Lookup(key3, now));

  // |key1| expires in 10 seconds.
  cache.Set(key1, entry, now, base::Seconds(10));
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
  now += base::Seconds(1);

  // |key2| expires before |key1| would originally have expired.
  cache.Set(key2, entry, now, base::Seconds(5));
  EXPECT_EQ(2u, cache.size());
  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.LookupStale(key1, now, &stale));
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_FALSE(cache.Lookup(key3, now));

  // |key1| should be chosen for eviction, since it is stale.
  cache.Set(key3, entry, now, base::Seconds(1));
  EXPECT_EQ(2u, cache.size());
  EXPECT_FALSE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.LookupStale(key1, now, &stale));
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_TRUE(cache.Lookup(key3, now));

  // Advance to t=6.
  now += base::Seconds(5);

  // Insert |key1| again. |key3| should be evicted.
  cache.Set(key1, entry, now, base::Seconds(10));
  EXPECT_EQ(2u, cache.size());
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_FALSE(cache.Lookup(key2, now));
  EXPECT_TRUE(cache.LookupStale(key2, now, &stale));
  EXPECT_FALSE(cache.Lookup(key3, now));
  EXPECT_FALSE(cache.LookupStale(key3, now, &stale));
}

// Pinned entries should not be evicted, even if the cache is full and the Entry
// has expired.
TEST(HostCacheTest, NoEvictPinned) {
  HostCache cache(2);

  base::TimeTicks now;

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Key key3 = Key("foobar3.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);
  entry.set_pinning(true);

  cache.Set(key1, entry, now, base::Seconds(5));
  now += base::Seconds(10);
  cache.Set(key2, entry, now, base::Seconds(5));
  now += base::Seconds(10);
  cache.Set(key3, entry, now, base::Seconds(5));

  // There are 3 entries in this cache whose nominal max size is 2.
  EXPECT_EQ(3u, cache.size());
  EXPECT_TRUE(cache.LookupStale(key1, now, nullptr));
  EXPECT_TRUE(cache.LookupStale(key2, now, nullptr));
  EXPECT_TRUE(cache.Lookup(key3, now));
}

// Obsolete pinned entries should be evicted normally.
TEST(HostCacheTest, EvictObsoletePinned) {
  HostCache cache(2);

  base::TimeTicks now;

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");
  HostCache::Key key3 = Key("foobar3.com");
  HostCache::Key key4 = Key("foobar4.com");
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);
  entry.set_pinning(true);

  // |key2| should be preserved, since it expires later.
  cache.Set(key1, entry, now, base::Seconds(5));
  cache.Set(key2, entry, now, base::Seconds(10));
  cache.Set(key3, entry, now, base::Seconds(5));
  // There are 3 entries in this cache whose nominal max size is 2.
  EXPECT_EQ(3u, cache.size());

  cache.Invalidate();
  // |Invalidate()| does not trigger eviction.
  EXPECT_EQ(3u, cache.size());

  // |Set()| triggers an eviction, leaving only |key2| in cache,
  // before adding |key4|
  cache.Set(key4, entry, now, base::Seconds(2));
  EXPECT_EQ(2u, cache.size());
  EXPECT_FALSE(cache.LookupStale(key1, now, nullptr));
  EXPECT_TRUE(cache.LookupStale(key2, now, nullptr));
  EXPECT_FALSE(cache.LookupStale(key3, now, nullptr));
  EXPECT_TRUE(cache.LookupStale(key4, now, nullptr));
}

// An active pin is preserved if the record is
// replaced due to a Set() call without the pin.
TEST(HostCacheTest, PreserveActivePin) {
  HostCache cache(2);

  base::TimeTicks now;

  // Make entry1 and entry2, identical except for IP and pinned flag.
  IPEndPoint endpoint1(IPAddress(192, 0, 2, 1), 0);
  IPEndPoint endpoint2(IPAddress(192, 0, 2, 2), 0);
  HostCache::Entry entry1 = HostCache::Entry(OK, {endpoint1}, /*aliases=*/{},
                                             HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry2 = HostCache::Entry(OK, {endpoint2}, /*aliases=*/{},
                                             HostCache::Entry::SOURCE_UNKNOWN);
  entry1.set_pinning(true);

  HostCache::Key key = Key("foobar.com");

  // Insert entry1, and verify that it can be retrieved with the
  // correct IP and |pinning()| == true.
  cache.Set(key, entry1, now, base::Seconds(10));
  const auto* pair1 = cache.Lookup(key, now);
  ASSERT_TRUE(pair1);
  const HostCache::Entry& result1 = pair1->second;
  EXPECT_THAT(result1.GetEndpoints(),
              ElementsAre(ExpectEndpointResult(ElementsAre(endpoint1))));
  EXPECT_THAT(result1.pinning(), Optional(true));

  // Insert |entry2|, and verify that it when it is retrieved, it
  // has the new IP, and the "pinned" flag copied from |entry1|.
  cache.Set(key, entry2, now, base::Seconds(10));
  const auto* pair2 = cache.Lookup(key, now);
  ASSERT_TRUE(pair2);
  const HostCache::Entry& result2 = pair2->second;
  EXPECT_THAT(result2.GetEndpoints(),
              ElementsAre(ExpectEndpointResult(ElementsAre(endpoint2))));
  EXPECT_THAT(result2.pinning(), Optional(true));
}

// An obsolete cache pin is not preserved if the record is replaced.
TEST(HostCacheTest, DontPreserveObsoletePin) {
  HostCache cache(2);

  base::TimeTicks now;

  // Make entry1 and entry2, identical except for IP and "pinned" flag.
  IPEndPoint endpoint1(IPAddress(192, 0, 2, 1), 0);
  IPEndPoint endpoint2(IPAddress(192, 0, 2, 2), 0);
  HostCache::Entry entry1 = HostCache::Entry(OK, {endpoint1}, /*aliases=*/{},
                                             HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry2 = HostCache::Entry(OK, {endpoint2}, /*aliases=*/{},
                                             HostCache::Entry::SOURCE_UNKNOWN);
  entry1.set_pinning(true);

  HostCache::Key key = Key("foobar.com");

  // Insert entry1, and verify that it can be retrieved with the
  // correct IP and |pinning()| == true.
  cache.Set(key, entry1, now, base::Seconds(10));
  const auto* pair1 = cache.Lookup(key, now);
  ASSERT_TRUE(pair1);
  const HostCache::Entry& result1 = pair1->second;
  EXPECT_THAT(result1.GetEndpoints(),
              ElementsAre(ExpectEndpointResult(ElementsAre(endpoint1))));
  EXPECT_THAT(result1.pinning(), Optional(true));

  // Make entry1 obsolete.
  cache.Invalidate();

  // Insert |entry2|, and verify that it when it is retrieved, it
  // has the new IP, and the "pinned" flag is not copied from |entry1|.
  cache.Set(key, entry2, now, base::Seconds(10));
  const auto* pair2 = cache.Lookup(key, now);
  ASSERT_TRUE(pair2);
  const HostCache::Entry& result2 = pair2->second;
  EXPECT_THAT(result2.GetEndpoints(),
              ElementsAre(ExpectEndpointResult(ElementsAre(endpoint2))));
  EXPECT_THAT(result2.pinning(), Optional(false));
}

// An active pin is removed if the record is replaced by a Set() call
// with the pin flag set to false.
TEST(HostCacheTest, Unpin) {
  HostCache cache(2);

  base::TimeTicks now;

  // Make entry1 and entry2, identical except for IP and pinned flag.
  IPEndPoint endpoint1(IPAddress(192, 0, 2, 1), 0);
  IPEndPoint endpoint2(IPAddress(192, 0, 2, 2), 0);
  HostCache::Entry entry1 = HostCache::Entry(OK, {endpoint1}, /*aliases=*/{},
                                             HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry2 = HostCache::Entry(OK, {endpoint2}, /*aliases=*/{},
                                             HostCache::Entry::SOURCE_UNKNOWN);
  entry1.set_pinning(true);
  entry2.set_pinning(false);

  HostCache::Key key = Key("foobar.com");

  // Insert entry1, and verify that it can be retrieved with the
  // correct IP and |pinning()| == true.
  cache.Set(key, entry1, now, base::Seconds(10));
  const auto* pair1 = cache.Lookup(key, now);
  ASSERT_TRUE(pair1);
  const HostCache::Entry& result1 = pair1->second;
  EXPECT_THAT(result1.GetEndpoints(),
              ElementsAre(ExpectEndpointResult(ElementsAre(endpoint1))));
  EXPECT_THAT(result1.pinning(), Optional(true));

  // Insert |entry2|, and verify that it when it is retrieved, it
  // has the new IP, and the "pinned" flag is now false.
  cache.Set(key, entry2, now, base::Seconds(10));
  const auto* pair2 = cache.Lookup(key, now);
  ASSERT_TRUE(pair2);
  const HostCache::Entry& result2 = pair2->second;
  EXPECT_THAT(result2.GetEndpoints(),
              ElementsAre(ExpectEndpointResult(ElementsAre(endpoint2))));
  EXPECT_THAT(result2.pinning(), Optional(false));
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
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       0},
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       1},
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       -1},
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host2", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       -1},
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host2", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       1},
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host2", 443),
                      DnsQueryType::A, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       -1},
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, HOST_RESOLVER_CANONNAME,
                      HostResolverSource::ANY, NetworkAnonymizationKey()),
       -1},
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, HOST_RESOLVER_CANONNAME,
                      HostResolverSource::ANY, NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       1},
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, HOST_RESOLVER_CANONNAME,
                      HostResolverSource::ANY, NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host2", 443),
                      DnsQueryType::UNSPECIFIED, HOST_RESOLVER_CANONNAME,
                      HostResolverSource::ANY, NetworkAnonymizationKey()),
       -1},
      // 9: Different host scheme.
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       1},
      // 10: Different host port.
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 1544),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       -1},
      // 11: Same host name without scheme/port.
      {HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkAnonymizationKey()),
       HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkAnonymizationKey()),
       0},
      // 12: Different host name without scheme/port.
      {HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkAnonymizationKey()),
       HostCache::Key("host2", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkAnonymizationKey()),
       -1},
      // 13: Only one with scheme/port.
      {HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                      DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                      NetworkAnonymizationKey()),
       HostCache::Key("host1", DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, NetworkAnonymizationKey()),
       -1},
  };
  HostCache::Key insecure_key =
      HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                     DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  HostCache::Key secure_key =
      HostCache::Key(url::SchemeHostPort(url::kHttpsScheme, "host1", 443),
                     DnsQueryType::UNSPECIFIED, 0, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  secure_key.secure = true;
  tests.emplace_back(insecure_key, secure_key, -1);

  for (size_t i = 0; i < std::size(tests); ++i) {
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

TEST(HostCacheTest, SerializeAndDeserializeWithExpirations) {
  const base::TimeDelta kTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;

  HostCache::Key expire_by_time_key = Key("expire.by.time.test");
  HostCache::Key expire_by_changes_key = Key("expire.by.changes.test");

  IPEndPoint endpoint(IPAddress(1, 2, 3, 4), 0);
  HostCache::Entry entry = HostCache::Entry(OK, {endpoint}, /*aliases=*/{},
                                            HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());

  // Add an entry for `expire_by_time_key` at t=0.
  EXPECT_FALSE(cache.Lookup(expire_by_time_key, now));
  cache.Set(expire_by_time_key, entry, now, kTTL);
  EXPECT_THAT(cache.Lookup(expire_by_time_key, now),
              Pointee(Pair(expire_by_time_key, EntryContentsEqual(entry))));

  EXPECT_EQ(1u, cache.size());

  // Advance to t=5.
  now += base::Seconds(5);

  // Add entry for `expire_by_changes_key` at t=5.
  EXPECT_FALSE(cache.Lookup(expire_by_changes_key, now));
  cache.Set(expire_by_changes_key, entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(expire_by_changes_key, now));
  EXPECT_EQ(2u, cache.size());

  EXPECT_EQ(0u, cache.last_restore_size());

  // Advance to t=12, and serialize/deserialize the cache.
  now += base::Seconds(7);

  base::Value::List serialized_cache;
  cache.GetList(serialized_cache, false /* include_staleness */,
                HostCache::SerializationType::kRestorable);
  HostCache restored_cache(kMaxCacheEntries);

  EXPECT_TRUE(restored_cache.RestoreFromListValue(serialized_cache));

  HostCache::EntryStaleness stale;

  // The `expire_by_time_key` entry is stale due to both network changes and
  // expiration time.
  EXPECT_FALSE(restored_cache.Lookup(expire_by_time_key, now));
  EXPECT_THAT(restored_cache.LookupStale(expire_by_time_key, now, &stale),
              Pointee(Pair(expire_by_time_key, EntryContentsEqual(entry))));
  EXPECT_EQ(1, stale.network_changes);
  // Time to TimeTicks conversion is fuzzy, so just check that expected and
  // actual expiration times are close.
  EXPECT_GT(base::Milliseconds(100),
            (base::Seconds(2) - stale.expired_by).magnitude());

  // The `expire_by_changes_key` entry is stale only due to network changes.
  EXPECT_FALSE(restored_cache.Lookup(expire_by_changes_key, now));
  EXPECT_THAT(restored_cache.LookupStale(expire_by_changes_key, now, &stale),
              Pointee(Pair(expire_by_changes_key, EntryContentsEqual(entry))));
  EXPECT_EQ(1, stale.network_changes);
  EXPECT_GT(base::Milliseconds(100),
            (base::Seconds(-3) - stale.expired_by).magnitude());

  EXPECT_EQ(2u, restored_cache.last_restore_size());
}

// Test that any changes between serialization and restore are preferred over
// old restored entries.
TEST(HostCacheTest, SerializeAndDeserializeWithChanges) {
  const base::TimeDelta kTTL = base::Seconds(10);

  HostCache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;

  HostCache::Key to_serialize_key1 = Key("to.serialize1.test");
  HostCache::Key to_serialize_key2 = Key("to.serialize2.test");
  HostCache::Key other_key = Key("other.test");

  IPEndPoint endpoint(IPAddress(1, 1, 1, 1), 0);
  HostCache::Entry serialized_entry = HostCache::Entry(
      OK, {endpoint}, /*aliases=*/{}, HostCache::Entry::SOURCE_UNKNOWN);

  IPEndPoint replacement_endpoint(IPAddress(2, 2, 2, 2), 0);
  HostCache::Entry replacement_entry =
      HostCache::Entry(OK, {replacement_endpoint}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);

  IPEndPoint other_endpoint(IPAddress(3, 3, 3, 3), 0);
  HostCache::Entry other_entry = HostCache::Entry(
      OK, {other_endpoint}, /*aliases=*/{}, HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());

  // Add `to_serialize_key1` and `to_serialize_key2`
  EXPECT_FALSE(cache.Lookup(to_serialize_key1, now));
  cache.Set(to_serialize_key1, serialized_entry, now, kTTL);
  EXPECT_THAT(
      cache.Lookup(to_serialize_key1, now),
      Pointee(Pair(to_serialize_key1, EntryContentsEqual(serialized_entry))));
  EXPECT_FALSE(cache.Lookup(to_serialize_key2, now));
  cache.Set(to_serialize_key2, serialized_entry, now, kTTL);
  EXPECT_THAT(
      cache.Lookup(to_serialize_key2, now),
      Pointee(Pair(to_serialize_key2, EntryContentsEqual(serialized_entry))));
  EXPECT_EQ(2u, cache.size());

  // Serialize the cache.
  base::Value::List serialized_cache;
  cache.GetList(serialized_cache, false /* include_staleness */,
                HostCache::SerializationType::kRestorable);
  HostCache restored_cache(kMaxCacheEntries);

  // Add entries for `to_serialize_key1` and `other_key` to the new cache
  // before restoring the serialized one. The `to_serialize_key1` result is
  // different from the original.
  EXPECT_FALSE(restored_cache.Lookup(to_serialize_key1, now));
  restored_cache.Set(to_serialize_key1, replacement_entry, now, kTTL);
  EXPECT_THAT(
      restored_cache.Lookup(to_serialize_key1, now),
      Pointee(Pair(to_serialize_key1, EntryContentsEqual(replacement_entry))));
  EXPECT_EQ(1u, restored_cache.size());

  EXPECT_FALSE(restored_cache.Lookup(other_key, now));
  restored_cache.Set(other_key, other_entry, now, kTTL);
  EXPECT_THAT(restored_cache.Lookup(other_key, now),
              Pointee(Pair(other_key, EntryContentsEqual(other_entry))));
  EXPECT_EQ(2u, restored_cache.size());

  EXPECT_EQ(0u, restored_cache.last_restore_size());

  EXPECT_TRUE(restored_cache.RestoreFromListValue(serialized_cache));
  EXPECT_EQ(1u, restored_cache.last_restore_size());

  HostCache::EntryStaleness stale;

  // Expect `to_serialize_key1` has the replacement entry.
  EXPECT_THAT(
      restored_cache.Lookup(to_serialize_key1, now),
      Pointee(Pair(to_serialize_key1, EntryContentsEqual(replacement_entry))));

  // Expect `to_serialize_key2` has the original entry.
  EXPECT_THAT(
      restored_cache.LookupStale(to_serialize_key2, now, &stale),
      Pointee(Pair(to_serialize_key2, EntryContentsEqual(serialized_entry))));

  // Expect no change for `other_key`.
  EXPECT_THAT(restored_cache.Lookup(other_key, now),
              Pointee(Pair(other_key, EntryContentsEqual(other_entry))));
}

TEST(HostCacheTest, SerializeAndDeserializeAddresses) {
  const base::TimeDelta kTTL = base::Seconds(10);

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

  HostCache::Entry entry1 = HostCache::Entry(
      OK, {endpoint_ipv4}, /*aliases=*/{}, HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry2 =
      HostCache::Entry(OK, {endpoint_ipv6, endpoint_ipv4}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry3 = HostCache::Entry(
      OK, {endpoint_ipv6}, /*aliases=*/{}, HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry entry4 = HostCache::Entry(
      OK, {endpoint_ipv4}, /*aliases=*/{}, HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(0u, cache.size());

  // Add an entry for "foobar.com" at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, entry1, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_TRUE(cache.Lookup(key1, now)->second.error() == entry1.error());

  EXPECT_EQ(1u, cache.size());

  // Advance to t=5.
  now += base::Seconds(5);

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
  now += base::Seconds(7);

  base::Value::List serialized_cache;
  cache.GetList(serialized_cache, false /* include_staleness */,
                HostCache::SerializationType::kRestorable);
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

  EXPECT_TRUE(restored_cache.RestoreFromListValue(serialized_cache));

  HostCache::EntryStaleness stale;

  // The "foobar.com" entry is stale due to both network changes and expiration
  // time.
  EXPECT_FALSE(restored_cache.Lookup(key1, now));
  const std::pair<const HostCache::Key, HostCache::Entry>* result1 =
      restored_cache.LookupStale(key1, now, &stale);
  EXPECT_TRUE(result1);
  EXPECT_TRUE(result1->first.secure);
  EXPECT_THAT(result1->second.text_records(), IsEmpty());
  EXPECT_THAT(result1->second.hostnames(), IsEmpty());
  EXPECT_EQ(1u, result1->second.ip_endpoints().size());
  EXPECT_EQ(endpoint_ipv4, result1->second.ip_endpoints().front());
  EXPECT_EQ(1, stale.network_changes);
  // Time to TimeTicks conversion is fuzzy, so just check that expected and
  // actual expiration times are close.
  EXPECT_GT(base::Milliseconds(100),
            (base::Seconds(2) - stale.expired_by).magnitude());

  // The "foobar2.com" entry is stale only due to network changes.
  EXPECT_FALSE(restored_cache.Lookup(key2, now));
  const std::pair<const HostCache::Key, HostCache::Entry>* result2 =
      restored_cache.LookupStale(key2, now, &stale);
  EXPECT_TRUE(result2);
  EXPECT_FALSE(result2->first.secure);
  EXPECT_EQ(2u, result2->second.ip_endpoints().size());
  EXPECT_EQ(endpoint_ipv6, result2->second.ip_endpoints().front());
  EXPECT_EQ(endpoint_ipv4, result2->second.ip_endpoints().back());
  EXPECT_EQ(1, stale.network_changes);
  EXPECT_GT(base::Milliseconds(100),
            (base::Seconds(-3) - stale.expired_by).magnitude());

  // The "foobar3.com" entry is the new one, not the restored one.
  const std::pair<const HostCache::Key, HostCache::Entry>* result3 =
      restored_cache.Lookup(key3, now);
  EXPECT_TRUE(result3);
  EXPECT_EQ(1u, result3->second.ip_endpoints().size());
  EXPECT_EQ(endpoint_ipv4, result3->second.ip_endpoints().front());

  // The "foobar4.com" entry is still present and usable.
  const std::pair<const HostCache::Key, HostCache::Entry>* result4 =
      restored_cache.Lookup(key4, now);
  EXPECT_TRUE(result4);
  EXPECT_EQ(1u, result4->second.ip_endpoints().size());
  EXPECT_EQ(endpoint_ipv4, result4->second.ip_endpoints().front());

  EXPECT_EQ(2u, restored_cache.last_restore_size());
}

TEST(HostCacheTest, SerializeAndDeserializeEntryWithoutScheme) {
  const base::TimeDelta kTTL = base::Seconds(10);

  HostCache::Key key("host.test", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkAnonymizationKey());
  HostCache::Entry entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{},
                       /*aliases=*/{}, HostCache::Entry::SOURCE_UNKNOWN);

  base::TimeTicks now;
  HostCache cache(kMaxCacheEntries);

  cache.Set(key, entry, now, kTTL);
  ASSERT_TRUE(cache.Lookup(key, now));
  ASSERT_EQ(cache.size(), 1u);

  base::Value::List serialized_cache;
  cache.GetList(serialized_cache, /*include_staleness=*/false,
                HostCache::SerializationType::kRestorable);
  HostCache restored_cache(kMaxCacheEntries);
  EXPECT_TRUE(restored_cache.RestoreFromListValue(serialized_cache));
  EXPECT_EQ(restored_cache.size(), 1u);

  HostCache::EntryStaleness staleness;
  EXPECT_THAT(restored_cache.LookupStale(key, now, &staleness),
              Pointee(Pair(key, EntryContentsEqual(entry))));
}

TEST(HostCacheTest, SerializeAndDeserializeWithNetworkAnonymizationKey) {
  const url::SchemeHostPort kHost =
      url::SchemeHostPort(url::kHttpsScheme, "hostname.test", 443);
  const base::TimeDelta kTTL = base::Seconds(10);
  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);
  const SchemefulSite kOpaqueSite;
  const auto kOpaqueNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kOpaqueSite);

  HostCache::Key key1(kHost, DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, kNetworkAnonymizationKey);
  HostCache::Key key2(kHost, DnsQueryType::UNSPECIFIED, 0,
                      HostResolverSource::ANY, kOpaqueNetworkAnonymizationKey);

  IPEndPoint endpoint(IPAddress(1, 2, 3, 4), 0);
  HostCache::Entry entry = HostCache::Entry(OK, {endpoint}, /*aliases=*/{},
                                            HostCache::Entry::SOURCE_UNKNOWN);

  base::TimeTicks now;
  HostCache cache(kMaxCacheEntries);

  cache.Set(key1, entry, now, kTTL);
  cache.Set(key2, entry, now, kTTL);

  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(kNetworkAnonymizationKey,
            cache.Lookup(key1, now)->first.network_anonymization_key);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(kOpaqueNetworkAnonymizationKey,
            cache.Lookup(key2, now)->first.network_anonymization_key);
  EXPECT_EQ(2u, cache.size());

  base::Value::List serialized_cache;
  cache.GetList(serialized_cache, false /* include_staleness */,
                HostCache::SerializationType::kRestorable);
  HostCache restored_cache(kMaxCacheEntries);
  EXPECT_TRUE(restored_cache.RestoreFromListValue(serialized_cache));
  EXPECT_EQ(1u, restored_cache.size());

  HostCache::EntryStaleness stale;
  EXPECT_THAT(restored_cache.LookupStale(key1, now, &stale),
              Pointee(Pair(key1, EntryContentsEqual(entry))));
  EXPECT_FALSE(restored_cache.Lookup(key2, now));
}

TEST(HostCacheTest, SerializeForDebugging) {
  const url::SchemeHostPort kHost(url::kHttpsScheme, "hostname.test", 443);
  const base::TimeDelta kTTL = base::Seconds(10);
  const NetworkAnonymizationKey kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateTransient();

  HostCache::Key key(kHost, DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, kNetworkAnonymizationKey);

  IPEndPoint endpoint(IPAddress(1, 2, 3, 4), 0);
  HostCache::Entry entry = HostCache::Entry(OK, {endpoint}, /*aliases=*/{},
                                            HostCache::Entry::SOURCE_UNKNOWN);

  base::TimeTicks now;
  HostCache cache(kMaxCacheEntries);

  cache.Set(key, entry, now, kTTL);

  EXPECT_TRUE(cache.Lookup(key, now));
  EXPECT_EQ(kNetworkAnonymizationKey,
            cache.Lookup(key, now)->first.network_anonymization_key);
  EXPECT_EQ(1u, cache.size());

  base::Value::List serialized_cache;
  cache.GetList(serialized_cache, false /* include_staleness */,
                HostCache::SerializationType::kDebug);
  HostCache restored_cache(kMaxCacheEntries);
  EXPECT_FALSE(restored_cache.RestoreFromListValue(serialized_cache));

  ASSERT_EQ(1u, serialized_cache.size());
  ASSERT_TRUE(serialized_cache[0].is_dict());
  const std::string* nak_string =
      serialized_cache[0].GetDict().FindString("network_anonymization_key");
  ASSERT_TRUE(nak_string);
  ASSERT_EQ(kNetworkAnonymizationKey.ToDebugString(), *nak_string);
}

TEST(HostCacheTest, SerializeAndDeserialize_Text) {
  base::TimeTicks now;

  base::TimeDelta ttl = base::Seconds(99);
  std::vector<std::string> text_records({"foo", "bar"});
  HostCache::Key key(url::SchemeHostPort(url::kHttpsScheme, "example.com", 443),
                     DnsQueryType::A, 0, HostResolverSource::DNS,
                     NetworkAnonymizationKey());
  key.secure = true;
  HostCache::Entry entry(OK, text_records, HostCache::Entry::SOURCE_DNS, ttl);
  EXPECT_THAT(entry.text_records(), Not(IsEmpty()));

  HostCache cache(kMaxCacheEntries);
  cache.Set(key, entry, now, ttl);
  EXPECT_EQ(1u, cache.size());

  base::Value::List serialized_cache;
  cache.GetList(serialized_cache, false /* include_staleness */,
                HostCache::SerializationType::kRestorable);
  HostCache restored_cache(kMaxCacheEntries);
  EXPECT_TRUE(restored_cache.RestoreFromListValue(serialized_cache));

  ASSERT_EQ(1u, serialized_cache.size());
  ASSERT_EQ(1u, restored_cache.size());
  HostCache::EntryStaleness stale;
  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      restored_cache.LookupStale(key, now, &stale);
  EXPECT_THAT(result, Pointee(Pair(key, EntryContentsEqual(entry))));
  EXPECT_THAT(result->second.text_records(), text_records);
}

TEST(HostCacheTest, SerializeAndDeserialize_Hostname) {
  base::TimeTicks now;

  base::TimeDelta ttl = base::Seconds(99);
  std::vector<HostPortPair> hostnames(
      {HostPortPair("example.com", 95), HostPortPair("chromium.org", 122)});
  HostCache::Key key(url::SchemeHostPort(url::kHttpsScheme, "example.com", 443),
                     DnsQueryType::A, 0, HostResolverSource::DNS,
                     NetworkAnonymizationKey());
  HostCache::Entry entry(OK, hostnames, HostCache::Entry::SOURCE_DNS, ttl);
  EXPECT_THAT(entry.hostnames(), Not(IsEmpty()));

  HostCache cache(kMaxCacheEntries);
  cache.Set(key, entry, now, ttl);
  EXPECT_EQ(1u, cache.size());

  base::Value::List serialized_cache;
  cache.GetList(serialized_cache, false /* include_staleness */,
                HostCache::SerializationType::kRestorable);
  HostCache restored_cache(kMaxCacheEntries);
  EXPECT_TRUE(restored_cache.RestoreFromListValue(serialized_cache));

  ASSERT_EQ(1u, restored_cache.size());
  HostCache::EntryStaleness stale;
  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      restored_cache.LookupStale(key, now, &stale);
  EXPECT_THAT(result, Pointee(Pair(key, EntryContentsEqual(entry))));
  EXPECT_THAT(result->second.hostnames(), hostnames);
}

TEST(HostCacheTest, SerializeAndDeserializeEndpointResult) {
  base::TimeTicks now;

  base::TimeDelta ttl = base::Seconds(99);
  HostCache::Key key(url::SchemeHostPort(url::kHttpsScheme, "example.com", 443),
                     DnsQueryType::A, 0, HostResolverSource::DNS,
                     NetworkAnonymizationKey());
  IPEndPoint ipv6_endpoint(
      IPAddress(1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4), 110);
  IPEndPoint ipv4_endpoint1(IPAddress(1, 1, 1, 1), 80);
  IPEndPoint ipv4_endpoint2(IPAddress(2, 2, 2, 2), 90);
  IPEndPoint other_ipv4_endpoint(IPAddress(3, 3, 3, 3), 100);
  std::string ipv6_alias = "ipv6_alias.test";
  std::string ipv4_alias = "ipv4_alias.test";
  std::string other_alias = "other_alias.test";
  std::vector<IPEndPoint> ip_endpoints = {ipv6_endpoint, ipv4_endpoint1,
                                          ipv4_endpoint2, other_ipv4_endpoint};
  std::set<std::string> aliases = {ipv6_alias, ipv4_alias, other_alias};
  HostCache::Entry entry(OK, ip_endpoints, aliases,
                         HostCache::Entry::SOURCE_DNS, ttl);

  std::set<std::string> canonical_names = {ipv6_alias, ipv4_alias};
  entry.set_canonical_names(canonical_names);

  EXPECT_THAT(entry.GetEndpoints(), Not(IsEmpty()));

  ConnectionEndpointMetadata metadata1;
  metadata1.supported_protocol_alpns = {"h3", "h2"};
  metadata1.ech_config_list = {'f', 'o', 'o'};
  metadata1.target_name = ipv6_alias;
  ConnectionEndpointMetadata metadata2;
  metadata2.supported_protocol_alpns = {"h2", "h4"};
  metadata2.target_name = ipv4_alias;
  HostCache::Entry metadata_entry(
      OK,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{
          {1u, metadata1}, {2u, metadata2}},
      HostCache::Entry::SOURCE_DNS);

  auto merged_entry = HostCache::Entry::MergeEntries(entry, metadata_entry);

  EXPECT_THAT(merged_entry.GetEndpoints(),
              ElementsAre(ExpectEndpointResult(ip_endpoints)));
  EXPECT_THAT(
      merged_entry.GetMetadatas(),
      testing::ElementsAre(
          ExpectConnectionEndpointMetadata(testing::ElementsAre("h3", "h2"),
                                           testing::ElementsAre('f', 'o', 'o'),
                                           ipv6_alias),
          ExpectConnectionEndpointMetadata(testing::ElementsAre("h2", "h4"),
                                           IsEmpty(), ipv4_alias)));
  EXPECT_THAT(merged_entry.canonical_names(),
              UnorderedElementsAre(ipv4_alias, ipv6_alias));

  HostCache cache(kMaxCacheEntries);
  cache.Set(key, merged_entry, now, ttl);
  EXPECT_EQ(1u, cache.size());

  base::Value::List serialized_cache;
  cache.GetList(serialized_cache, false /* include_staleness */,
                HostCache::SerializationType::kRestorable);
  HostCache restored_cache(kMaxCacheEntries);
  EXPECT_TRUE(restored_cache.RestoreFromListValue(serialized_cache));

  // Check `serialized_cache` can be encoded as JSON. This ensures it has no
  // binary values.
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(serialized_cache, &json));

  ASSERT_EQ(1u, restored_cache.size());
  HostCache::EntryStaleness stale;
  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      restored_cache.LookupStale(key, now, &stale);

  ASSERT_TRUE(result);
  EXPECT_THAT(result, Pointee(Pair(key, EntryContentsEqual(merged_entry))));
  EXPECT_THAT(result->second.GetEndpoints(),
              ElementsAre(ExpectEndpointResult(ip_endpoints)));
  EXPECT_THAT(
      result->second.GetMetadatas(),
      testing::ElementsAre(
          ExpectConnectionEndpointMetadata(testing::ElementsAre("h3", "h2"),
                                           testing::ElementsAre('f', 'o', 'o'),
                                           ipv6_alias),
          ExpectConnectionEndpointMetadata(testing::ElementsAre("h2", "h4"),
                                           IsEmpty(), ipv4_alias)));
  EXPECT_THAT(result->second.canonical_names(),
              UnorderedElementsAre(ipv4_alias, ipv6_alias));

  EXPECT_EQ(result->second.aliases(), aliases);
}

TEST(HostCacheTest, DeserializeNoEndpointNoAliase) {
  base::TimeDelta ttl = base::Seconds(99);
  std::string expiration_time_str = base::NumberToString(
      (base::Time::Now() + ttl).since_origin().InMicroseconds());

  auto dict = base::JSONReader::Read(base::StringPrintf(
      R"(
 [ {
   "dns_query_type": 1,
   "expiration": "%s",
   "flags": 0,
   "host_resolver_source": 2,
   "hostname": "example.com",
   "network_anonymization_key": [  ],
   "port": 443,
   "scheme": "https",
   "secure": false
} ]
)",
      expiration_time_str.c_str()));
  ASSERT_TRUE(dict);

  HostCache restored_cache(kMaxCacheEntries);
  ASSERT_TRUE(dict->is_list());
  EXPECT_TRUE(restored_cache.RestoreFromListValue(dict->GetList()));

  ASSERT_EQ(1u, restored_cache.size());

  HostCache::Key key(url::SchemeHostPort(url::kHttpsScheme, "example.com", 443),
                     DnsQueryType::A, 0, HostResolverSource::DNS,
                     NetworkAnonymizationKey());

  HostCache::EntryStaleness stale;
  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      restored_cache.LookupStale(key, base::TimeTicks::Now(), &stale);

  ASSERT_TRUE(result);
  EXPECT_THAT(result->second.aliases(), ElementsAre());
  EXPECT_THAT(result->second.ip_endpoints(), ElementsAre());
}

TEST(HostCacheTest, DeserializeLegacyAddresses) {
  base::TimeDelta ttl = base::Seconds(99);
  std::string expiration_time_str = base::NumberToString(
      (base::Time::Now() + ttl).since_origin().InMicroseconds());

  auto dict = base::JSONReader::Read(base::StringPrintf(
      R"(
 [ {
   "addresses": [ "2000::", "1.2.3.4" ],
   "dns_query_type": 1,
   "expiration": "%s",
   "flags": 0,
   "host_resolver_source": 2,
   "hostname": "example.com",
   "network_anonymization_key": [  ],
   "port": 443,
   "scheme": "https",
   "secure": false
} ]
)",
      expiration_time_str.c_str()));
  ASSERT_TRUE(dict);

  HostCache restored_cache(kMaxCacheEntries);
  ASSERT_TRUE(dict->is_list());
  EXPECT_TRUE(restored_cache.RestoreFromListValue(dict->GetList()));

  ASSERT_EQ(1u, restored_cache.size());

  HostCache::Key key(url::SchemeHostPort(url::kHttpsScheme, "example.com", 443),
                     DnsQueryType::A, 0, HostResolverSource::DNS,
                     NetworkAnonymizationKey());

  HostCache::EntryStaleness stale;
  const std::pair<const HostCache::Key, HostCache::Entry>* result =
      restored_cache.LookupStale(key, base::TimeTicks::Now(), &stale);

  ASSERT_TRUE(result);
  EXPECT_THAT(result->second.ip_endpoints(),
              ElementsAreArray(MakeEndpoints({"2000::", "1.2.3.4"})));
  EXPECT_THAT(result->second.aliases(), ElementsAre());
}

TEST(HostCacheTest, DeserializeInvalidQueryTypeIntegrity) {
  base::TimeDelta ttl = base::Seconds(99);
  std::string expiration_time_str = base::NumberToString(
      (base::Time::Now() + ttl).since_origin().InMicroseconds());

  // RestoreFromListValue doesn't support dns_query_type=6 (INTEGRITY).
  auto dict = base::JSONReader::Read(base::StringPrintf(
      R"(
 [ {
   "addresses": [ "2000::", "1.2.3.4" ],
   "dns_query_type": 6,
   "expiration": "%s",
   "flags": 0,
   "host_resolver_source": 2,
   "hostname": "example.com",
   "network_anonymization_key": [  ],
   "port": 443,
   "scheme": "https",
   "secure": false
} ]
)",
      expiration_time_str.c_str()));
  ASSERT_TRUE(dict);

  HostCache restored_cache(kMaxCacheEntries);
  ASSERT_TRUE(dict->is_list());
  EXPECT_FALSE(restored_cache.RestoreFromListValue(dict->GetList()));

  ASSERT_EQ(0u, restored_cache.size());
}

TEST(HostCacheTest, DeserializeInvalidQueryTypeHttpsExperimental) {
  base::TimeDelta ttl = base::Seconds(99);
  std::string expiration_time_str = base::NumberToString(
      (base::Time::Now() + ttl).since_origin().InMicroseconds());

  // RestoreFromListValue doesn't support dns_query_type=8 (HTTPS_EXPERIMENTAL).
  auto dict = base::JSONReader::Read(base::StringPrintf(
      R"(
 [ {
   "addresses": [ "2000::", "1.2.3.4" ],
   "dns_query_type": 8,
   "expiration": "%s",
   "flags": 0,
   "host_resolver_source": 2,
   "hostname": "example.com",
   "network_anonymization_key": [  ],
   "port": 443,
   "scheme": "https",
   "secure": false
} ]
)",
      expiration_time_str.c_str()));
  ASSERT_TRUE(dict);

  HostCache restored_cache(kMaxCacheEntries);
  ASSERT_TRUE(dict->is_list());
  EXPECT_FALSE(restored_cache.RestoreFromListValue(dict->GetList()));

  ASSERT_EQ(0u, restored_cache.size());
}

TEST(HostCacheTest, PersistenceDelegate) {
  const base::TimeDelta kTTL = base::Seconds(10);
  HostCache cache(kMaxCacheEntries);
  MockPersistenceDelegate delegate;
  cache.set_persistence_delegate(&delegate);

  HostCache::Key key1 = Key("foobar.com");
  HostCache::Key key2 = Key("foobar2.com");

  HostCache::Entry ok_entry =
      HostCache::Entry(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                       HostCache::Entry::SOURCE_UNKNOWN);
  std::vector<IPEndPoint> other_endpoints = {
      IPEndPoint(IPAddress(1, 1, 1, 1), 300)};
  HostCache::Entry other_entry(OK, std::move(other_endpoints), /*aliases=*/{},
                               HostCache::Entry::SOURCE_UNKNOWN);
  HostCache::Entry error_entry =
      HostCache::Entry(ERR_NAME_NOT_RESOLVED, /*ip_endpoints=*/{},
                       /*aliases=*/{}, HostCache::Entry::SOURCE_UNKNOWN);

  // Start at t=0.
  base::TimeTicks now;
  EXPECT_EQ(0u, cache.size());

  // Add two entries at t=0.
  EXPECT_FALSE(cache.Lookup(key1, now));
  cache.Set(key1, ok_entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(1, delegate.num_changes());

  EXPECT_FALSE(cache.Lookup(key2, now));
  cache.Set(key2, error_entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key2, now));
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(2, delegate.num_changes());

  // Advance to t=5.
  now += base::Seconds(5);

  // Changes that shouldn't trigger a write:
  // Add an entry for "foobar.com" with different expiration time.
  EXPECT_TRUE(cache.Lookup(key1, now));
  cache.Set(key1, ok_entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(2, delegate.num_changes());

  // Add an entry for "foobar.com" with different TTL.
  EXPECT_TRUE(cache.Lookup(key1, now));
  cache.Set(key1, ok_entry, now, kTTL - base::Seconds(5));
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(2, delegate.num_changes());

  // Changes that should trigger a write:
  // Add an entry for "foobar.com" with different address list.
  EXPECT_TRUE(cache.Lookup(key1, now));
  cache.Set(key1, other_entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(3, delegate.num_changes());

  // Add an entry for "foobar2.com" with different error.
  EXPECT_TRUE(cache.Lookup(key1, now));
  cache.Set(key2, ok_entry, now, kTTL);
  EXPECT_TRUE(cache.Lookup(key1, now));
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(4, delegate.num_changes());
}

TEST(HostCacheTest, MergeEndpointsWithAliases) {
  const IPAddress kAddressFront(1, 2, 3, 4);
  const IPEndPoint kEndpointFront(kAddressFront, 0);
  HostCache::Entry front(OK, {kEndpointFront}, {"alias1", "alias2", "alias3"},
                         HostCache::Entry::SOURCE_DNS);
  front.set_text_records(std::vector<std::string>{"text1"});
  const HostPortPair kHostnameFront("host", 1);
  front.set_hostnames(std::vector<HostPortPair>{kHostnameFront});

  const IPAddress kAddressBack(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0);
  const IPEndPoint kEndpointBack(kAddressBack, 0);
  HostCache::Entry back(OK, {kEndpointBack}, {"alias2", "alias4", "alias5"},
                        HostCache::Entry::SOURCE_DNS);
  back.set_text_records(std::vector<std::string>{"text2"});
  const HostPortPair kHostnameBack("host", 2);
  back.set_hostnames(std::vector<HostPortPair>{kHostnameBack});

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  EXPECT_THAT(result.ip_endpoints(),
              ElementsAre(kEndpointFront, kEndpointBack));
  EXPECT_THAT(result.text_records(), ElementsAre("text1", "text2"));

  EXPECT_THAT(result.hostnames(), ElementsAre(kHostnameFront, kHostnameBack));

  EXPECT_THAT(
      result.aliases(),
      UnorderedElementsAre("alias1", "alias2", "alias3", "alias4", "alias5"));
}

TEST(HostCacheTest, MergeEndpointsKeepEndpointsOrder) {
  std::vector<IPEndPoint> front_addresses =
      MakeEndpoints({"::1", "0.0.0.2", "0.0.0.4"});
  std::vector<IPEndPoint> back_addresses =
      MakeEndpoints({"0.0.0.2", "0.0.0.2", "::3", "::3", "0.0.0.4"});

  HostCache::Entry front(OK, front_addresses, /*aliases=*/{"front"},
                         HostCache::Entry::SOURCE_DNS);
  HostCache::Entry back(OK, back_addresses, /*aliases=*/{"back"},
                        HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_THAT(
      result.ip_endpoints(),
      ElementsAreArray(MakeEndpoints({"::1", "0.0.0.2", "0.0.0.4", "0.0.0.2",
                                      "0.0.0.2", "::3", "::3", "0.0.0.4"})));
  EXPECT_THAT(result.aliases(), UnorderedElementsAre("front", "back"));
}

TEST(HostCacheTest, MergeMetadatas) {
  ConnectionEndpointMetadata front_metadata;
  front_metadata.supported_protocol_alpns = {"h5", "h6", "monster truck rally"};
  front_metadata.ech_config_list = {'h', 'i'};
  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
      front_metadata_map{{4u, front_metadata}};
  HostCache::Entry front(OK, front_metadata_map, HostCache::Entry::SOURCE_DNS);

  ConnectionEndpointMetadata back_metadata;
  back_metadata.supported_protocol_alpns = {"h5"};
  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
      back_metadata_map{{2u, back_metadata}};
  HostCache::Entry back(OK, back_metadata_map, HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result = HostCache::Entry::MergeEntries(front, back);

  // Expect `GetEndpoints()` to ignore metadatas if no `IPEndPoint`s.
  EXPECT_THAT(result.GetEndpoints(), IsEmpty());

  // Expect order irrelevant for endpoint metadata merging.
  result = HostCache::Entry::MergeEntries(back, front);
  EXPECT_THAT(result.GetEndpoints(), IsEmpty());
}

TEST(HostCacheTest, MergeMetadatasWithIpEndpointsDifferentCanonicalName) {
  std::string target_name = "example.com";
  std::string other_target_name = "other.example.com";
  ConnectionEndpointMetadata metadata;
  metadata.supported_protocol_alpns = {"h5", "h6", "monster truck rally"};
  metadata.ech_config_list = {'h', 'i'};
  metadata.target_name = target_name;

  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadata_map{
      {4u, metadata}};
  HostCache::Entry metadata_entry(OK, metadata_map,
                                  HostCache::Entry::SOURCE_DNS);

  // Expect `GetEndpoints()` to always ignore metadatas with no `IPEndPoint`s.
  EXPECT_THAT(metadata_entry.GetEndpoints(), IsEmpty());

  // Merge in an `IPEndPoint` with different canonical name.
  IPEndPoint ip_endpoint(IPAddress(1, 1, 1, 1), 0);
  HostCache::Entry with_ip_endpoint(OK, {ip_endpoint}, /*aliases=*/{},
                                    HostCache::Entry::SOURCE_DNS);
  with_ip_endpoint.set_canonical_names(
      std::set<std::string>{other_target_name});
  HostCache::Entry result =
      HostCache::Entry::MergeEntries(metadata_entry, with_ip_endpoint);

  // Expect `GetEndpoints()` not to return the metadata.
  EXPECT_THAT(
      result.GetEndpoints(),
      ElementsAre(ExpectEndpointResult(std::vector<IPEndPoint>{ip_endpoint})));

  // Expect merge order irrelevant.
  EXPECT_EQ(result,
            HostCache::Entry::MergeEntries(with_ip_endpoint, metadata_entry));
}

TEST(HostCacheTest, MergeMetadatasWithIpEndpointsMatchingCanonicalName) {
  std::string target_name = "example.com";
  ConnectionEndpointMetadata metadata;
  metadata.supported_protocol_alpns = {"h5", "h6", "monster truck rally"};
  metadata.ech_config_list = {'h', 'i'};
  metadata.target_name = target_name;

  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadata_map{
      {4u, metadata}};
  HostCache::Entry metadata_entry(OK, metadata_map,
                                  HostCache::Entry::SOURCE_DNS);

  // Expect `GetEndpoints()` to always ignore metadatas with no `IPEndPoint`s.
  EXPECT_THAT(metadata_entry.GetEndpoints(), IsEmpty());

  // Merge in an `IPEndPoint` with different canonical name.
  IPEndPoint ip_endpoint(IPAddress(1, 1, 1, 1), 0);
  HostCache::Entry with_ip_endpoint(OK, {ip_endpoint}, /*aliases=*/{},
                                    HostCache::Entry::SOURCE_DNS);
  with_ip_endpoint.set_canonical_names(std::set<std::string>{target_name});
  HostCache::Entry result =
      HostCache::Entry::MergeEntries(metadata_entry, with_ip_endpoint);

  // Expect `GetEndpoints()` to return the metadata.
  EXPECT_THAT(
      result.GetEndpoints(),
      ElementsAre(ExpectEndpointResult(ElementsAre(ip_endpoint), metadata),
                  ExpectEndpointResult(ElementsAre(ip_endpoint))));

  // Expect merge order irrelevant.
  EXPECT_EQ(result,
            HostCache::Entry::MergeEntries(with_ip_endpoint, metadata_entry));
}

TEST(HostCacheTest, MergeMultipleMetadatasWithIpEndpoints) {
  std::string target_name = "example.com";
  ConnectionEndpointMetadata front_metadata;
  front_metadata.supported_protocol_alpns = {"h5", "h6", "monster truck rally"};
  front_metadata.ech_config_list = {'h', 'i'};
  front_metadata.target_name = target_name;

  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
      front_metadata_map{{4u, front_metadata}};
  HostCache::Entry front(OK, front_metadata_map, HostCache::Entry::SOURCE_DNS);

  ConnectionEndpointMetadata back_metadata;
  back_metadata.supported_protocol_alpns = {"h5"};
  back_metadata.target_name = target_name;
  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
      back_metadata_map{{2u, back_metadata}};
  HostCache::Entry back(OK, back_metadata_map, HostCache::Entry::SOURCE_DNS);

  HostCache::Entry merged_metadatas =
      HostCache::Entry::MergeEntries(front, back);
  HostCache::Entry reversed_merged_metadatas =
      HostCache::Entry::MergeEntries(back, front);

  // Expect `GetEndpoints()` to always ignore metadatas with no `IPEndPoint`s.
  EXPECT_THAT(merged_metadatas.GetEndpoints(), IsEmpty());
  EXPECT_THAT(reversed_merged_metadatas.GetEndpoints(), IsEmpty());

  // Merge in an `IPEndPoint`.
  IPEndPoint ip_endpoint(IPAddress(1, 1, 1, 1), 0);
  HostCache::Entry with_ip_endpoint(OK, {ip_endpoint}, /*aliases=*/{},
                                    HostCache::Entry::SOURCE_DNS);
  with_ip_endpoint.set_canonical_names(std::set<std::string>{target_name});

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(merged_metadatas, with_ip_endpoint);

  // Expect `back_metadata` before `front_metadata` because it has lower
  // priority number.
  EXPECT_THAT(
      result.GetEndpoints(),
      ElementsAre(
          ExpectEndpointResult(ElementsAre(ip_endpoint), back_metadata),
          ExpectEndpointResult(ElementsAre(ip_endpoint), front_metadata),
          ExpectEndpointResult(ElementsAre(ip_endpoint))));

  // Expect merge order irrelevant.
  EXPECT_EQ(result, HostCache::Entry::MergeEntries(reversed_merged_metadatas,
                                                   with_ip_endpoint));
  EXPECT_EQ(result,
            HostCache::Entry::MergeEntries(with_ip_endpoint, merged_metadatas));
  EXPECT_EQ(result, HostCache::Entry::MergeEntries(with_ip_endpoint,
                                                   reversed_merged_metadatas));
}

TEST(HostCacheTest, MergeAliases) {
  HostCache::Entry front(OK, /*ip_endpoints=*/{},
                         /*aliases=*/{"foo1.test", "foo2.test", "foo3.test"},
                         HostCache::Entry::SOURCE_DNS);

  HostCache::Entry back(OK, /*ip_endpoints=*/{},
                        /*aliases=*/{"foo2.test", "foo4.test"},
                        HostCache::Entry::SOURCE_DNS);

  HostCache::Entry expected(
      OK, /*ip_endpoints=*/{},
      /*aliases=*/{"foo1.test", "foo2.test", "foo3.test", "foo4.test"},
      HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result = HostCache::Entry::MergeEntries(front, back);
  EXPECT_EQ(result, expected);

  // Expect order irrelevant for alias merging.
  result = HostCache::Entry::MergeEntries(back, front);
  EXPECT_EQ(result, expected);
}

TEST(HostCacheTest, MergeEntries_frontEmpty) {
  HostCache::Entry front(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);

  const IPAddress kAddressBack(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0);
  const IPEndPoint kEndpointBack(kAddressBack, 0);
  HostCache::Entry back(OK, {kEndpointBack}, {"alias1", "alias2", "alias3"},
                        HostCache::Entry::SOURCE_DNS, base::Hours(4));
  back.set_text_records(std::vector<std::string>{"text2"});
  const HostPortPair kHostnameBack("host", 2);
  back.set_hostnames(std::vector<HostPortPair>{kHostnameBack});

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  EXPECT_THAT(result.ip_endpoints(), ElementsAre(kEndpointBack));
  EXPECT_THAT(result.text_records(), ElementsAre("text2"));
  EXPECT_THAT(result.hostnames(), ElementsAre(kHostnameBack));

  EXPECT_EQ(base::Hours(4), result.ttl());

  EXPECT_THAT(result.aliases(),
              UnorderedElementsAre("alias1", "alias2", "alias3"));
}

TEST(HostCacheTest, MergeEntries_backEmpty) {
  const IPAddress kAddressFront(1, 2, 3, 4);
  const IPEndPoint kEndpointFront(kAddressFront, 0);
  HostCache::Entry front(OK, {kEndpointFront}, {"alias1", "alias2", "alias3"},
                         HostCache::Entry::SOURCE_DNS, base::Minutes(5));
  front.set_text_records(std::vector<std::string>{"text1"});
  const HostPortPair kHostnameFront("host", 1);
  front.set_hostnames(std::vector<HostPortPair>{kHostnameFront});

  HostCache::Entry back(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  EXPECT_THAT(result.ip_endpoints(), ElementsAre(kEndpointFront));
  EXPECT_THAT(result.text_records(), ElementsAre("text1"));
  EXPECT_THAT(result.hostnames(), ElementsAre(kHostnameFront));

  EXPECT_EQ(base::Minutes(5), result.ttl());

  EXPECT_THAT(result.aliases(),
              UnorderedElementsAre("alias1", "alias2", "alias3"));
}

TEST(HostCacheTest, MergeEntries_bothEmpty) {
  HostCache::Entry front(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);
  HostCache::Entry back(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  EXPECT_THAT(result.ip_endpoints(), IsEmpty());
  EXPECT_THAT(result.text_records(), IsEmpty());
  EXPECT_THAT(result.hostnames(), IsEmpty());
  EXPECT_FALSE(result.has_ttl());
}

TEST(HostCacheTest, MergeEntries_frontWithAliasesNoAddressesBackWithBoth) {
  HostCache::Entry front(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);
  std::set<std::string> aliases_front({"alias0", "alias1", "alias2"});
  front.set_aliases(aliases_front);

  const IPAddress kAddressBack(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0);
  const IPEndPoint kEndpointBack(kAddressBack, 0);
  HostCache::Entry back(OK, {kEndpointBack}, {"alias1", "alias2", "alias3"},
                        HostCache::Entry::SOURCE_DNS, base::Hours(4));

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  EXPECT_THAT(result.ip_endpoints(), ElementsAre(kEndpointBack));

  EXPECT_EQ(base::Hours(4), result.ttl());

  EXPECT_THAT(result.aliases(),
              UnorderedElementsAre("alias0", "alias1", "alias2", "alias3"));
}

TEST(HostCacheTest, MergeEntries_backWithAliasesNoAddressesFrontWithBoth) {
  HostCache::Entry back(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS);
  std::set<std::string> aliases_back({"alias1", "alias2", "alias3"});
  back.set_aliases(aliases_back);

  const IPAddress kAddressFront(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0);
  const IPEndPoint kEndpointFront(kAddressFront, 0);
  HostCache::Entry front(OK, {kEndpointFront}, {"alias0", "alias1", "alias2"},
                         HostCache::Entry::SOURCE_DNS, base::Hours(4));

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  EXPECT_THAT(result.ip_endpoints(), ElementsAre(kEndpointFront));

  EXPECT_EQ(base::Hours(4), result.ttl());

  EXPECT_THAT(result.aliases(),
              UnorderedElementsAre("alias0", "alias1", "alias2", "alias3"));
}

TEST(HostCacheTest, MergeEntries_frontWithAddressesNoAliasesBackWithBoth) {
  const IPAddress kAddressFront(1, 2, 3, 4);
  const IPEndPoint kEndpointFront(kAddressFront, 0);
  HostCache::Entry front(OK, {kEndpointFront}, /*aliases=*/{},
                         HostCache::Entry::SOURCE_DNS, base::Hours(4));

  const IPAddress kAddressBack(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0);
  const IPEndPoint kEndpointBack(kAddressBack, 0);
  HostCache::Entry back(OK, {kEndpointBack}, {"alias1", "alias2", "alias3"},
                        HostCache::Entry::SOURCE_DNS, base::Hours(4));
  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  EXPECT_THAT(result.ip_endpoints(),
              ElementsAre(kEndpointFront, kEndpointBack));

  EXPECT_EQ(base::Hours(4), result.ttl());

  EXPECT_THAT(result.aliases(),
              UnorderedElementsAre("alias1", "alias2", "alias3"));
}

TEST(HostCacheTest, MergeEntries_backWithAddressesNoAliasesFrontWithBoth) {
  const IPAddress kAddressFront(1, 2, 3, 4);
  const IPEndPoint kEndpointFront(kAddressFront, 0);
  HostCache::Entry front(OK, {kEndpointFront}, {"alias1", "alias2", "alias3"},
                         HostCache::Entry::SOURCE_DNS, base::Hours(4));
  const IPAddress kAddressBack(0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0);
  const IPEndPoint kEndpointBack(kAddressBack, 0);
  HostCache::Entry back(OK, {kEndpointBack}, /*aliases=*/{},
                        HostCache::Entry::SOURCE_DNS, base::Hours(4));

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(OK, result.error());
  EXPECT_EQ(HostCache::Entry::SOURCE_DNS, result.source());

  EXPECT_THAT(result.ip_endpoints(),
              ElementsAre(kEndpointFront, kEndpointBack));

  EXPECT_EQ(base::Hours(4), result.ttl());

  EXPECT_THAT(result.aliases(),
              UnorderedElementsAre("alias1", "alias2", "alias3"));
}

TEST(HostCacheTest, MergeEntries_differentTtl) {
  HostCache::Entry front(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS,
                         base::Days(12));
  HostCache::Entry back(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS,
                        base::Seconds(42));

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_EQ(base::Seconds(42), result.ttl());
}

TEST(HostCacheTest, MergeEntries_FrontCannonnamePreserved) {
  HostCache::Entry front(OK, /*ip_endpoints=*/{}, /*aliases=*/{"name1"},
                         HostCache::Entry::SOURCE_DNS);

  HostCache::Entry back(OK, /*ip_endpoints=*/{}, /*aliases=*/{"name2"},
                        HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_THAT(result.aliases(), UnorderedElementsAre("name1", "name2"));
}

// Test that the back canonname can be used if there is no front cannonname.
TEST(HostCacheTest, MergeEntries_BackCannonnameUsable) {
  HostCache::Entry front(OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                         HostCache::Entry::SOURCE_DNS);

  HostCache::Entry back(OK, /*ip_endpoints=*/{}, /*aliases=*/{"name2"},
                        HostCache::Entry::SOURCE_DNS);

  HostCache::Entry result =
      HostCache::Entry::MergeEntries(std::move(front), std::move(back));

  EXPECT_THAT(result.aliases(), UnorderedElementsAre("name2"));
}

TEST(HostCacheTest, ConvertFromInternalAddressResult) {
  const std::vector<IPEndPoint> kEndpoints{
      IPEndPoint(IPAddress(2, 2, 2, 2), 46)};
  constexpr base::TimeDelta kTtl1 = base::Minutes(45);
  constexpr base::TimeDelta kTtl2 = base::Minutes(40);
  constexpr base::TimeDelta kTtl3 = base::Minutes(55);

  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  results.insert(std::make_unique<HostResolverInternalDataResult>(
      "endpoint.test", DnsQueryType::AAAA, base::TimeTicks() + kTtl1,
      base::Time() + kTtl1, HostResolverInternalResult::Source::kDns,
      kEndpoints, std::vector<std::string>{}, std::vector<HostPortPair>{}));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain1.test", DnsQueryType::AAAA, base::TimeTicks() + kTtl2,
      base::Time() + kTtl2, HostResolverInternalResult::Source::kDns,
      "domain2.test"));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain2.test", DnsQueryType::AAAA, base::TimeTicks() + kTtl3,
      base::Time() + kTtl3, HostResolverInternalResult::Source::kDns,
      "endpoint.test"));

  HostCache::Entry converted(std::move(results), base::Time(),
                             base::TimeTicks());

  // Expect kTtl2 because it is the min TTL.
  HostCache::Entry expected(
      OK, kEndpoints,
      /*aliases=*/{"domain1.test", "domain2.test", "endpoint.test"},
      HostCache::Entry::SOURCE_DNS, kTtl2);
  expected.set_canonical_names(std::set<std::string>{"endpoint.test"});

  // Entries converted from HostResolverInternalDataResults do not differentiate
  // between empty and no-data for the various data types, so need to set empty
  // strings and hostname entries into `expected`.
  expected.set_text_records(std::vector<std::string>());
  expected.set_hostnames(std::vector<HostPortPair>());

  EXPECT_EQ(converted, expected);
}

TEST(HostCacheTest, ConvertFromInternalMetadataResult) {
  const std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
      kMetadatas{{1, ConnectionEndpointMetadata({"h2", "h3"},
                                                /*ech_config_list=*/{},
                                                "target.test")}};
  constexpr base::TimeDelta kTtl1 = base::Minutes(45);
  constexpr base::TimeDelta kTtl2 = base::Minutes(40);
  constexpr base::TimeDelta kTtl3 = base::Minutes(55);

  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  results.insert(std::make_unique<HostResolverInternalMetadataResult>(
      "endpoint.test", DnsQueryType::HTTPS, base::TimeTicks() + kTtl1,
      base::Time() + kTtl1, HostResolverInternalResult::Source::kDns,
      kMetadatas));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain1.test", DnsQueryType::HTTPS, base::TimeTicks() + kTtl2,
      base::Time() + kTtl2, HostResolverInternalResult::Source::kDns,
      "domain2.test"));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain2.test", DnsQueryType::HTTPS, base::TimeTicks() + kTtl3,
      base::Time() + kTtl3, HostResolverInternalResult::Source::kDns,
      "endpoint.test"));

  HostCache::Entry converted(std::move(results), base::Time(),
                             base::TimeTicks());

  // Expect kTtl2 because it is the min TTL.
  HostCache::Entry expected(OK, kMetadatas, HostCache::Entry::SOURCE_DNS,
                            kTtl2);
  expected.set_https_record_compatibility(std::vector<bool>{true});

  EXPECT_EQ(converted, expected);
}

// Test the case of compatible HTTPS records but no metadata of use to Chrome.
// Represented in internal result type as an empty metadata result. Represented
// in HostCache::Entry as empty metadata with at least one true in
// `https_record_compatibility_`.
TEST(HostCacheTest, ConvertFromCompatibleOnlyInternalMetadataResult) {
  const std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
      kMetadatas;
  constexpr base::TimeDelta kTtl1 = base::Minutes(45);
  constexpr base::TimeDelta kTtl2 = base::Minutes(40);
  constexpr base::TimeDelta kTtl3 = base::Minutes(55);

  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  results.insert(std::make_unique<HostResolverInternalMetadataResult>(
      "endpoint.test", DnsQueryType::HTTPS, base::TimeTicks() + kTtl1,
      base::Time() + kTtl1, HostResolverInternalResult::Source::kDns,
      kMetadatas));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain1.test", DnsQueryType::HTTPS, base::TimeTicks() + kTtl2,
      base::Time() + kTtl2, HostResolverInternalResult::Source::kDns,
      "domain2.test"));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain2.test", DnsQueryType::HTTPS, base::TimeTicks() + kTtl3,
      base::Time() + kTtl3, HostResolverInternalResult::Source::kDns,
      "endpoint.test"));

  HostCache::Entry converted(std::move(results), base::Time(),
                             base::TimeTicks());

  // Expect kTtl2 because it is the min TTL.
  HostCache::Entry expected(ERR_NAME_NOT_RESOLVED, kMetadatas,
                            HostCache::Entry::SOURCE_DNS, kTtl2);
  expected.set_https_record_compatibility(std::vector<bool>{true});

  EXPECT_EQ(converted, expected);
}

TEST(HostCacheTest, ConvertFromInternalErrorResult) {
  constexpr base::TimeDelta kTtl1 = base::Minutes(45);
  constexpr base::TimeDelta kTtl2 = base::Minutes(40);
  constexpr base::TimeDelta kTtl3 = base::Minutes(55);

  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  results.insert(std::make_unique<HostResolverInternalErrorResult>(
      "endpoint.test", DnsQueryType::A, base::TimeTicks() + kTtl1,
      base::Time() + kTtl1, HostResolverInternalResult::Source::kDns,
      ERR_NAME_NOT_RESOLVED));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain1.test", DnsQueryType::A, base::TimeTicks() + kTtl2,
      base::Time() + kTtl2, HostResolverInternalResult::Source::kDns,
      "domain2.test"));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain2.test", DnsQueryType::A, base::TimeTicks() + kTtl3,
      base::Time() + kTtl3, HostResolverInternalResult::Source::kDns,
      "endpoint.test"));

  HostCache::Entry converted(std::move(results), base::Time(),
                             base::TimeTicks());

  // Expect kTtl2 because it is the min TTL.
  HostCache::Entry expected(ERR_NAME_NOT_RESOLVED, HostCache::Entry::SOURCE_DNS,
                            kTtl2);

  EXPECT_EQ(converted, expected);
}

TEST(HostCacheTest, ConvertFromNonCachableInternalErrorResult) {
  constexpr base::TimeDelta kTtl1 = base::Minutes(45);
  constexpr base::TimeDelta kTtl2 = base::Minutes(40);

  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  results.insert(std::make_unique<HostResolverInternalErrorResult>(
      "endpoint.test", DnsQueryType::AAAA, /*expiration=*/std::nullopt,
      /*timed_expiration=*/std::nullopt,
      HostResolverInternalResult::Source::kDns, ERR_NAME_NOT_RESOLVED));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain1.test", DnsQueryType::AAAA, base::TimeTicks() + kTtl1,
      base::Time() + kTtl1, HostResolverInternalResult::Source::kDns,
      "domain2.test"));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain2.test", DnsQueryType::AAAA, base::TimeTicks() + kTtl2,
      base::Time() + kTtl2, HostResolverInternalResult::Source::kDns,
      "endpoint.test"));

  HostCache::Entry converted(std::move(results), base::Time(),
                             base::TimeTicks());

  // Expect no TTL because error is non-cachable (has no TTL itself).
  HostCache::Entry expected(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_DNS);

  EXPECT_EQ(converted, expected);
}

TEST(HostCacheTest, ConvertFromInternalAliasOnlyResult) {
  constexpr base::TimeDelta kTtl1 = base::Minutes(45);
  constexpr base::TimeDelta kTtl2 = base::Minutes(40);

  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain1.test", DnsQueryType::A, base::TimeTicks() + kTtl1,
      base::Time() + kTtl1, HostResolverInternalResult::Source::kDns,
      "domain2.test"));
  results.insert(std::make_unique<HostResolverInternalAliasResult>(
      "domain2.test", DnsQueryType::A, base::TimeTicks() + kTtl2,
      base::Time() + kTtl2, HostResolverInternalResult::Source::kDns,
      "endpoint.test"));

  HostCache::Entry converted(std::move(results), base::Time(),
                             base::TimeTicks());

  // Expect no TTL because alias-only results are not cacheable.
  HostCache::Entry expected(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_DNS);

  EXPECT_EQ(converted, expected);
}

TEST(HostCacheTest, ConvertFromEmptyInternalResult) {
  HostCache::Entry converted({}, base::Time(), base::TimeTicks());
  HostCache::Entry expected(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);

  EXPECT_EQ(converted, expected);
}

}  // namespace net
