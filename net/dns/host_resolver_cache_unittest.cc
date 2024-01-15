// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_cache.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/connection_endpoint_metadata_test_util.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/host_resolver_internal_result_test_util.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;

MATCHER(IsNotStale, "") {
  return !arg.IsStale() && !arg.expired_by.has_value() &&
         !arg.stale_by_generation;
}

MATCHER_P(IsNotStale, result_matcher, "") {
  return !arg.IsStale() && !arg.expired_by.has_value() &&
         !arg.stale_by_generation &&
         ExplainMatchResult(result_matcher, arg.result.get(), result_listener);
}

// Fudge TimeDelta matching by a couple milliseconds because it is not important
// whether something is considered expired at or just after expiration because
// TTLs come at second-wide precision anyway.
MATCHER_P(TimeDeltaIsApproximately, approximate_expectation, "") {
  return arg - base::Milliseconds(3) <= approximate_expectation &&
         arg + base::Milliseconds(3) >= approximate_expectation;
}

MATCHER_P2(IsStale, expired_by_matcher, expected_stale_by_generation, "") {
  return arg.IsStale() &&
         ExplainMatchResult(expired_by_matcher, arg.expired_by,
                            result_listener) &&
         arg.stale_by_generation == expected_stale_by_generation;
}

MATCHER_P3(IsStale,
           result_matcher,
           expired_by_matcher,
           expected_stale_by_generation,
           "") {
  return arg.IsStale() &&
         ExplainMatchResult(result_matcher, arg.result.get(),
                            result_listener) &&
         ExplainMatchResult(expired_by_matcher, arg.expired_by,
                            result_listener) &&
         arg.stale_by_generation == expected_stale_by_generation;
}

class HostResolverCacheTest : public ::testing::Test {
 protected:
  const size_t kMaxResults = 10;

  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
};

TEST_F(HostResolverCacheTest, CacheAResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress(1, 2, 3, 4), /*port=*/0),
      IPEndPoint(IPAddress(2, 3, 4, 5), /*port=*/0)};
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::A, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  auto matcher = Pointee(ExpectHostResolverInternalDataResult(
      kName, DnsQueryType::A, HostResolverInternalResult::Source::kDns,
      Optional(tick_clock_.NowTicks() + kTtl), Optional(clock_.Now() + kTtl),
      kEndpoints));
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::A,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::UNSPECIFIED,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::A,
                           HostResolverSource::ANY, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::A,
                           HostResolverSource::DNS, /*secure=*/std::nullopt),
              matcher);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                         HostResolverSource::DNS, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::A,
                         HostResolverSource::SYSTEM, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::A,
                         HostResolverSource::DNS, /*secure=*/true),
            nullptr);

  auto stale_result_matcher =
      Optional(IsNotStale(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::A, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints)));
  EXPECT_THAT(cache.LookupStale(kName, anonymization_key, DnsQueryType::A,
                                HostResolverSource::DNS, /*secure=*/false),
              stale_result_matcher);
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::UNSPECIFIED,
                        HostResolverSource::DNS, /*secure=*/false),
      stale_result_matcher);
  EXPECT_THAT(cache.LookupStale(kName, anonymization_key, DnsQueryType::A,
                                HostResolverSource::ANY, /*secure=*/false),
              stale_result_matcher);
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::A,
                        HostResolverSource::DNS, /*secure=*/std::nullopt),
      stale_result_matcher);
  EXPECT_EQ(cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                              HostResolverSource::DNS, /*secure=*/false),
            std::nullopt);
  EXPECT_EQ(cache.LookupStale(kName, anonymization_key, DnsQueryType::A,
                              HostResolverSource::SYSTEM, /*secure=*/false),
            std::nullopt);
  EXPECT_EQ(cache.LookupStale(kName, anonymization_key, DnsQueryType::A,
                              HostResolverSource::DNS, /*secure=*/true),
            std::nullopt);
}

TEST_F(HostResolverCacheTest, CacheAaaaResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0),
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  auto matcher = Pointee(ExpectHostResolverInternalDataResult(
      kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
      Optional(tick_clock_.NowTicks() + kTtl), Optional(clock_.Now() + kTtl),
      kEndpoints));
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::UNSPECIFIED,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                           HostResolverSource::ANY, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                           HostResolverSource::DNS, /*secure=*/std::nullopt),
              matcher);

  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::A,
                         HostResolverSource::DNS, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                         HostResolverSource::SYSTEM, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                         HostResolverSource::DNS, /*secure=*/true),
            nullptr);

  auto stale_result_matcher =
      Optional(IsNotStale(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints)));
  EXPECT_THAT(cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                                HostResolverSource::DNS, /*secure=*/false),
              stale_result_matcher);
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::UNSPECIFIED,
                        HostResolverSource::DNS, /*secure=*/false),
      stale_result_matcher);
  EXPECT_THAT(cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                                HostResolverSource::ANY, /*secure=*/false),
              stale_result_matcher);
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::DNS, /*secure=*/std::nullopt),
      stale_result_matcher);

  EXPECT_EQ(cache.LookupStale(kName, anonymization_key, DnsQueryType::A,
                              HostResolverSource::DNS, /*secure=*/false),
            std::nullopt);
  EXPECT_EQ(cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                              HostResolverSource::SYSTEM, /*secure=*/false),
            std::nullopt);
  EXPECT_EQ(cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                              HostResolverSource::DNS, /*secure=*/true),
            std::nullopt);
}

TEST_F(HostResolverCacheTest, CacheHttpsResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
      kMetadatas = {
          {2, ConnectionEndpointMetadata({"h2", "h3"},
                                         /*ech_config_list=*/{}, kName)},
          {1,
           ConnectionEndpointMetadata({"h2"}, /*ech_config_list=*/{}, kName)}};
  auto result = std::make_unique<HostResolverInternalMetadataResult>(
      kName, DnsQueryType::HTTPS, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kMetadatas);

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  auto matcher = Pointee(ExpectHostResolverInternalMetadataResult(
      kName, DnsQueryType::HTTPS, HostResolverInternalResult::Source::kDns,
      Optional(tick_clock_.NowTicks() + kTtl), Optional(clock_.Now() + kTtl),
      kMetadatas));
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::UNSPECIFIED,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS,
                           HostResolverSource::ANY, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS,
                           HostResolverSource::DNS, /*secure=*/std::nullopt),
              matcher);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::A,
                         HostResolverSource::DNS, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS,
                         HostResolverSource::SYSTEM, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS,
                         HostResolverSource::DNS, /*secure=*/true),
            nullptr);

  auto stale_result_matcher =
      Optional(IsNotStale(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kMetadatas)));
  EXPECT_THAT(cache.LookupStale(kName, anonymization_key, DnsQueryType::HTTPS,
                                HostResolverSource::DNS, /*secure=*/false),
              stale_result_matcher);
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::UNSPECIFIED,
                        HostResolverSource::DNS, /*secure=*/false),
      stale_result_matcher);
  EXPECT_THAT(cache.LookupStale(kName, anonymization_key, DnsQueryType::HTTPS,
                                HostResolverSource::ANY, /*secure=*/false),
              stale_result_matcher);
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::HTTPS,
                        HostResolverSource::DNS, /*secure=*/std::nullopt),
      stale_result_matcher);
  EXPECT_EQ(cache.LookupStale(kName, anonymization_key, DnsQueryType::A,
                              HostResolverSource::DNS, /*secure=*/false),
            std::nullopt);
  EXPECT_EQ(cache.LookupStale(kName, anonymization_key, DnsQueryType::HTTPS,
                              HostResolverSource::SYSTEM, /*secure=*/false),
            std::nullopt);
  EXPECT_EQ(cache.LookupStale(kName, anonymization_key, DnsQueryType::HTTPS,
                              HostResolverSource::DNS, /*secure=*/true),
            std::nullopt);
}

// Domain names containing scheme/port are not expected to be handled any
// differently from other domain names. That is, if an entry is cached with
// a domain name containing scheme or port, it can only be looked up using the
// exact same domain name containing scheme and port. Testing the case simply
// because such things were handled differently in a previous version of the
// cache.
TEST_F(HostResolverCacheTest, RespectsSchemeAndPortInName) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kNameWithScheme = "_411._https.foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::string kAlpn1 = "foo";
  auto result1 = std::make_unique<HostResolverInternalMetadataResult>(
      kNameWithScheme, DnsQueryType::HTTPS, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{
          {4, ConnectionEndpointMetadata({kAlpn1}, /*ech_config_list=*/{},
                                         kNameWithScheme)}});

  const std::string kNameWithoutScheme = "foo.test";
  const std::string kAlpn2 = "bar";
  auto result2 = std::make_unique<HostResolverInternalMetadataResult>(
      kNameWithoutScheme, DnsQueryType::HTTPS, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{
          {7, ConnectionEndpointMetadata({kAlpn2}, /*ech_config_list=*/{},
                                         kNameWithoutScheme)}});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);
  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(cache.Lookup(kNameWithScheme, anonymization_key),
              Pointee(ExpectHostResolverInternalMetadataResult(
                  kNameWithScheme, DnsQueryType::HTTPS,
                  HostResolverInternalResult::Source::kDns,
                  /*expiration_matcher=*/Ne(std::nullopt),
                  /*timed_expiration_matcher=*/Ne(std::nullopt),
                  ElementsAre(Pair(4, ExpectConnectionEndpointMetadata(
                                          ElementsAre(kAlpn1), IsEmpty(),
                                          kNameWithScheme))))));
  EXPECT_THAT(cache.Lookup(kNameWithoutScheme, anonymization_key),
              Pointee(ExpectHostResolverInternalMetadataResult(
                  kNameWithoutScheme, DnsQueryType::HTTPS,
                  HostResolverInternalResult::Source::kDns,
                  /*expiration_matcher=*/Ne(std::nullopt),
                  /*timed_expiration_matcher=*/Ne(std::nullopt),
                  ElementsAre(Pair(7, ExpectConnectionEndpointMetadata(
                                          ElementsAre(kAlpn2), IsEmpty(),
                                          kNameWithoutScheme))))));
}

TEST_F(HostResolverCacheTest, CacheHttpsAliasResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::string kTarget = "target.test";
  auto result = std::make_unique<HostResolverInternalAliasResult>(
      kName, DnsQueryType::HTTPS, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns, kTarget);

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  auto matcher = Pointee(ExpectHostResolverInternalAliasResult(
      kName, DnsQueryType::HTTPS, HostResolverInternalResult::Source::kDns,
      Optional(tick_clock_.NowTicks() + kTtl), Optional(clock_.Now() + kTtl),
      kTarget));
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::UNSPECIFIED,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS,
                           HostResolverSource::ANY, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS,
                           HostResolverSource::DNS, /*secure=*/std::nullopt),
              matcher);

  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::A,
                         HostResolverSource::DNS, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS,
                         HostResolverSource::SYSTEM, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS,
                         HostResolverSource::DNS, /*secure=*/true),
            nullptr);
}

TEST_F(HostResolverCacheTest, CacheCnameAliasResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::string kTarget = "target.test";

  // CNAME results are not typically queried directly, but received as part of
  // the results for queries for other query types. Thus except in the weird
  // cases where it is queried directly, CNAME results should be cached for the
  // queried type (or as a wildcard UNSPECIFIED type), rather than type CNAME.
  // Here, test the case where it is cached under the AAAA query type.
  auto result = std::make_unique<HostResolverInternalAliasResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns, kTarget);

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  auto matcher = Pointee(ExpectHostResolverInternalAliasResult(
      kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
      Optional(tick_clock_.NowTicks() + kTtl), Optional(clock_.Now() + kTtl),
      kTarget));
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::UNSPECIFIED,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                           HostResolverSource::ANY, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                           HostResolverSource::DNS, /*secure=*/std::nullopt),
              matcher);

  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::A,
                         HostResolverSource::DNS, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                         HostResolverSource::SYSTEM, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                         HostResolverSource::DNS, /*secure=*/true),
            nullptr);
}

TEST_F(HostResolverCacheTest, CacheWildcardAlias) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::string kAliasTarget = "target.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  auto result = std::make_unique<HostResolverInternalAliasResult>(
      kName, DnsQueryType::UNSPECIFIED, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kAliasTarget);

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  auto matcher = Pointee(ExpectHostResolverInternalAliasResult(
      kName, DnsQueryType::UNSPECIFIED,
      HostResolverInternalResult::Source::kDns,
      Optional(tick_clock_.NowTicks() + kTtl), Optional(clock_.Now() + kTtl),
      kAliasTarget));
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::UNSPECIFIED),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::A), matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::TXT),
              matcher);
}

TEST_F(HostResolverCacheTest, CacheErrorResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  auto result = std::make_unique<HostResolverInternalErrorResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      ERR_NAME_NOT_RESOLVED);

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  auto matcher = Pointee(ExpectHostResolverInternalErrorResult(
      kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
      Optional(tick_clock_.NowTicks() + kTtl), Optional(clock_.Now() + kTtl),
      ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::UNSPECIFIED,
                           HostResolverSource::DNS, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                           HostResolverSource::ANY, /*secure=*/false),
              matcher);
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                           HostResolverSource::DNS, /*secure=*/std::nullopt),
              matcher);

  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::A,
                         HostResolverSource::DNS, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                         HostResolverSource::SYSTEM, /*secure=*/false),
            nullptr);
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                         HostResolverSource::DNS, /*secure=*/true),
            nullptr);
}

TEST_F(HostResolverCacheTest, ResultsCanBeUpdated) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kEndpoints1 = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result1 = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints1,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::string kName2 = "goo.test";
  auto result2 = std::make_unique<HostResolverInternalDataResult>(
      kName2, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints1,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);
  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints1)));
  EXPECT_THAT(
      cache.Lookup(kName2, anonymization_key),
      Pointee(ExpectHostResolverInternalDataResult(
          kName2, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints1)));

  const std::vector<IPEndPoint> kEndpoints2 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto result3 = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints2,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  cache.Set(std::move(result3), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints2)));
  EXPECT_THAT(
      cache.Lookup(kName2, anonymization_key),
      Pointee(ExpectHostResolverInternalDataResult(
          kName2, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints1)));
}

TEST_F(HostResolverCacheTest, UpdateCanReplaceWildcard) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::string kAliasTarget1 = "target1.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  auto result1 = std::make_unique<HostResolverInternalAliasResult>(
      kName, DnsQueryType::UNSPECIFIED, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kAliasTarget1);

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_NE(cache.Lookup(kName, anonymization_key, DnsQueryType::A), nullptr);
  EXPECT_NE(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA),
            nullptr);

  const std::string kAliasTarget2 = "target2.test";
  auto result2 = std::make_unique<HostResolverInternalAliasResult>(
      kName, DnsQueryType::A, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kAliasTarget2);

  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // After update, because most recent entry is not wildcard, expect lookup to
  // only succeed for the specific type.
  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key, DnsQueryType::A),
      Pointee(ExpectHostResolverInternalAliasResult(
          kName, DnsQueryType::A, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kAliasTarget2)));
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA),
            nullptr);
}

TEST_F(HostResolverCacheTest, WildcardUpdateCanReplaceSpecifics) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::string kAliasTarget1 = "target1.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  auto result1 = std::make_unique<HostResolverInternalAliasResult>(
      kName, DnsQueryType::A, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kAliasTarget1);
  const std::string kAliasTarget2 = "target2.test";
  auto result2 = std::make_unique<HostResolverInternalAliasResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kAliasTarget2);

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);
  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key, DnsQueryType::A),
      Pointee(ExpectHostResolverInternalAliasResult(
          kName, DnsQueryType::A, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kAliasTarget1)));
  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA),
      Pointee(ExpectHostResolverInternalAliasResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kAliasTarget2)));
  EXPECT_EQ(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS),
            nullptr);

  const std::string kAliasTarget3 = "target3.test";
  auto result3 = std::make_unique<HostResolverInternalAliasResult>(
      kName, DnsQueryType::UNSPECIFIED, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kAliasTarget3);

  cache.Set(std::move(result3), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::A),
              Pointee(ExpectHostResolverInternalAliasResult(
                  kName, DnsQueryType::UNSPECIFIED,
                  HostResolverInternalResult::Source::kDns,
                  Optional(tick_clock_.NowTicks() + kTtl),
                  Optional(clock_.Now() + kTtl), kAliasTarget3)));
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA),
              Pointee(ExpectHostResolverInternalAliasResult(
                  kName, DnsQueryType::UNSPECIFIED,
                  HostResolverInternalResult::Source::kDns,
                  Optional(tick_clock_.NowTicks() + kTtl),
                  Optional(clock_.Now() + kTtl), kAliasTarget3)));
  EXPECT_THAT(cache.Lookup(kName, anonymization_key, DnsQueryType::HTTPS),
              Pointee(ExpectHostResolverInternalAliasResult(
                  kName, DnsQueryType::UNSPECIFIED,
                  HostResolverInternalResult::Source::kDns,
                  Optional(tick_clock_.NowTicks() + kTtl),
                  Optional(clock_.Now() + kTtl), kAliasTarget3)));
}

TEST_F(HostResolverCacheTest, LookupNameIsCanonicalized) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "fOO.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_NE(cache.Lookup("FOO.TEST", anonymization_key), nullptr);
}

TEST_F(HostResolverCacheTest, LookupIgnoresExpiredResults) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName1 = "foo.test";
  const base::TimeDelta kTtl1 = base::Minutes(2);
  const std::vector<IPEndPoint> kEndpoints1 = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result1 = std::make_unique<HostResolverInternalDataResult>(
      kName1, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl1,
      clock_.Now() + kTtl1, HostResolverInternalResult::Source::kDns,
      kEndpoints1,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const std::string kName2 = "bar.test";
  const base::TimeDelta kTtl2 = base::Minutes(4);
  const std::vector<IPEndPoint> kEndpoints2 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto result2 = std::make_unique<HostResolverInternalDataResult>(
      kName2, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl2,
      clock_.Now() + kTtl2, HostResolverInternalResult::Source::kDns,
      kEndpoints2,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);
  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(
      cache.Lookup(kName1, anonymization_key),
      Pointee(ExpectHostResolverInternalDataResult(
          kName1, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl1),
          Optional(clock_.Now() + kTtl1), kEndpoints1)));
  EXPECT_THAT(cache.LookupStale(kName1, anonymization_key),
              Optional(IsNotStale()));
  EXPECT_THAT(
      cache.Lookup(kName2, anonymization_key),
      Pointee(ExpectHostResolverInternalDataResult(
          kName2, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl2),
          Optional(clock_.Now() + kTtl2), kEndpoints2)));
  EXPECT_THAT(cache.LookupStale(kName2, anonymization_key),
              Optional(IsNotStale()));

  // Advance time until just before first expiration. Expect both results still
  // active.
  clock_.Advance(kTtl1 - base::Milliseconds(1));
  tick_clock_.Advance(kTtl1 - base::Milliseconds(1));
  EXPECT_NE(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName1, anonymization_key),
              Optional(IsNotStale()));
  EXPECT_NE(cache.Lookup(kName2, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName2, anonymization_key),
              Optional(IsNotStale()));

  // Advance time until just after first expiration. Expect first result now
  // stale, but second result still valid.
  clock_.Advance(base::Milliseconds(2));
  tick_clock_.Advance(base::Milliseconds(2));
  EXPECT_EQ(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_THAT(
      cache.LookupStale(kName1, anonymization_key),
      Optional(IsStale(
          ExpectHostResolverInternalDataResult(
              kName1, DnsQueryType::AAAA,
              HostResolverInternalResult::Source::kDns, Ne(std::nullopt),
              Ne(std::nullopt), kEndpoints1),
          Optional(TimeDeltaIsApproximately(base::Milliseconds(1))), false)));
  EXPECT_NE(cache.Lookup(kName2, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName2, anonymization_key),
              Optional(IsNotStale()));

  // Advance time util just before second expiration. Expect first still stale
  // and second still valid.
  clock_.Advance(kTtl2 - kTtl1 - base::Milliseconds(2));
  tick_clock_.Advance(kTtl2 - kTtl1 - base::Milliseconds(2));
  EXPECT_EQ(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName1, anonymization_key),
              Optional(IsStale(Optional(TimeDeltaIsApproximately(
                                   base::Minutes(2) - base::Milliseconds(1))),
                               false)));
  EXPECT_NE(cache.Lookup(kName2, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName2, anonymization_key),
              Optional(IsNotStale()));

  // Advance time to after second expiration. Expect both results now stale.
  clock_.Advance(base::Milliseconds(2));
  tick_clock_.Advance(base::Milliseconds(2));
  EXPECT_EQ(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName1, anonymization_key),
              Optional(IsStale(Optional(TimeDeltaIsApproximately(
                                   base::Minutes(2) + base::Milliseconds(1))),
                               false)));
  EXPECT_EQ(cache.Lookup(kName2, anonymization_key), nullptr);
  EXPECT_THAT(
      cache.LookupStale(kName2, anonymization_key),
      Optional(IsStale(
          ExpectHostResolverInternalDataResult(
              kName2, DnsQueryType::AAAA,
              HostResolverInternalResult::Source::kDns, Ne(std::nullopt),
              Ne(std::nullopt), kEndpoints2),
          Optional(TimeDeltaIsApproximately(base::Milliseconds(1))), false)));
}

TEST_F(HostResolverCacheTest, ExpiredResultsCanBeUpdated) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Milliseconds(1),
      clock_.Now() - base::Milliseconds(1),
      HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expiration before Now, so expect entry to start expired.
  EXPECT_EQ(cache.Lookup(kName, anonymization_key), nullptr);
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key),
      Optional(IsStale(
          Optional(TimeDeltaIsApproximately(base::Milliseconds(1))), false)));

  const base::TimeDelta kTtl = base::Seconds(45);
  auto update_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(update_result), anonymization_key,
            HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_NE(cache.Lookup(kName, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName, anonymization_key),
              Optional(IsNotStale()));

  // Expect entry to still be expirable for new TTL.
  clock_.Advance(kTtl + base::Milliseconds(1));
  tick_clock_.Advance(kTtl + base::Milliseconds(1));
  EXPECT_EQ(cache.Lookup(kName, anonymization_key), nullptr);
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key),
      Optional(IsStale(
          Optional(TimeDeltaIsApproximately(base::Milliseconds(1))), false)));
}

TEST_F(HostResolverCacheTest, LookupIgnoresResultsMarkedStale) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName1 = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kEndpoints1 = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result1 = std::make_unique<HostResolverInternalDataResult>(
      kName1, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints1,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const std::string kName2 = "bar.test";
  const std::vector<IPEndPoint> kEndpoints2 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto result2 = std::make_unique<HostResolverInternalDataResult>(
      kName2, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints2,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);
  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_NE(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName1, anonymization_key),
              Optional(IsNotStale()));
  EXPECT_NE(cache.Lookup(kName2, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName2, anonymization_key),
              Optional(IsNotStale()));

  cache.MakeAllResultsStale();

  // Expect both entries to now be stale.
  EXPECT_EQ(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName1, anonymization_key),
              Optional(IsStale(std::nullopt, true)));
  EXPECT_EQ(cache.Lookup(kName2, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName2, anonymization_key),
              Optional(IsStale(std::nullopt, true)));

  const std::string kName3 = "foo3.test";
  const std::vector<IPEndPoint> kEndpoints3 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::2").value(),
                 /*port=*/0)};
  auto result3 = std::make_unique<HostResolverInternalDataResult>(
      kName3, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints3,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result3), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_EQ(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName1, anonymization_key),
              Optional(IsStale(std::nullopt, true)));
  EXPECT_EQ(cache.Lookup(kName2, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName2, anonymization_key),
              Optional(IsStale(std::nullopt, true)));
  EXPECT_THAT(
      cache.Lookup(kName3, anonymization_key),
      Pointee(ExpectHostResolverInternalDataResult(
          kName3, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints3)));
  EXPECT_THAT(cache.LookupStale(kName3, anonymization_key),
              Optional(IsNotStale()));
}

TEST_F(HostResolverCacheTest, MarkedStaleResultsCanBeUpdated) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(6);
  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  cache.MakeAllResultsStale();

  EXPECT_EQ(cache.Lookup(kName, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName, anonymization_key),
              Optional(IsStale(std::nullopt, true)));

  auto update_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(update_result), anonymization_key,
            HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_NE(cache.Lookup(kName, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName, anonymization_key),
              Optional(IsNotStale()));
}

TEST_F(HostResolverCacheTest, RespectsNetworkAnonymizationKey) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(5);
  const std::vector<IPEndPoint> kEndpoints1 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(), /*port=*/0)};
  auto result1 = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints1,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kEndpoints2 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::10").value(), /*port=*/0)};
  auto result2 = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints2,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const SchemefulSite kSite1(GURL("https://site1.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://site2.test/"));
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  cache.Set(std::move(result1), kNetworkAnonymizationKey1,
            HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_NE(cache.Lookup(kName, kNetworkAnonymizationKey1), nullptr);
  EXPECT_NE(cache.LookupStale(kName, kNetworkAnonymizationKey1), std::nullopt);
  EXPECT_EQ(cache.Lookup(kName, kNetworkAnonymizationKey2), nullptr);
  EXPECT_EQ(cache.LookupStale(kName, kNetworkAnonymizationKey2), std::nullopt);

  cache.Set(std::move(result2), kNetworkAnonymizationKey2,
            HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(
      cache.Lookup(kName, kNetworkAnonymizationKey1),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints1)));
  EXPECT_THAT(
      cache.LookupStale(kName, kNetworkAnonymizationKey1),
      Optional(IsNotStale(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints1))));
  EXPECT_THAT(
      cache.Lookup(kName, kNetworkAnonymizationKey2),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints2)));
  EXPECT_THAT(
      cache.LookupStale(kName, kNetworkAnonymizationKey2),
      Optional(IsNotStale(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kEndpoints2))));
}

// Newly added entries are always considered to be the most up-to-date
// information, so if an unexpired entry is updated with an expired entry, the
// entry should now be expired.
TEST_F(HostResolverCacheTest, UpdateToStale) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Hours(2),
      clock_.Now() + base::Hours(2), HostResolverInternalResult::Source::kDns,
      kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect initial entry to be unexpired.
  EXPECT_NE(cache.Lookup(kName, anonymization_key), nullptr);
  EXPECT_THAT(cache.LookupStale(kName, anonymization_key),
              Optional(IsNotStale()));

  auto update_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Seconds(1),
      clock_.Now() - base::Seconds(1), HostResolverInternalResult::Source::kDns,
      kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(update_result), anonymization_key,
            HostResolverSource::DNS,
            /*secure=*/false);

  // Expect entry to be expired.
  EXPECT_EQ(cache.Lookup(kName, anonymization_key), nullptr);
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key),
      Optional(IsStale(Optional(TimeDeltaIsApproximately(base::Seconds(1))),
                       false)));
}

// If a wildcard lookup matches multiple result entries, all insecure, expect
// lookup to return the most recently set result.
TEST_F(HostResolverCacheTest, PreferMoreRecentInsecureResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kNewEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::8").value(),
                 /*port=*/0)};
  auto new_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kNewEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kOldEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::7").value(),
                 /*port=*/0)};
  auto old_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kOldEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;

  cache.Set(std::move(old_result), anonymization_key,
            HostResolverSource::SYSTEM,
            /*secure=*/false);
  cache.Set(std::move(new_result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                   HostResolverSource::ANY, /*secure=*/false),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kNewEndpoints)));

  // Other result still available for more specific lookups.
  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                   HostResolverSource::SYSTEM, /*secure=*/false),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kOldEndpoints)));
}

// If a wildcard lookup matches multiple result entries, all secure, expect
// lookup to return the most recently set result.
TEST_F(HostResolverCacheTest, PreferMoreRecentSecureResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kNewEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::8").value(),
                 /*port=*/0)};
  auto new_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kNewEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kOldEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::7").value(),
                 /*port=*/0)};
  auto old_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kOldEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;

  cache.Set(std::move(old_result), anonymization_key,
            HostResolverSource::SYSTEM,
            /*secure=*/true);
  cache.Set(std::move(new_result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/true);

  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                   HostResolverSource::ANY, /*secure=*/true),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kNewEndpoints)));

  // Other result still available for more specific lookups.
  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                   HostResolverSource::SYSTEM, /*secure=*/true),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kOldEndpoints)));
}

// If a wildcard lookup matches multiple result entries of mixed secureness,
// expect lookup to return the most recently set secure result.
TEST_F(HostResolverCacheTest, PreferMoreSecureResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kInsecureEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto insecure_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kInsecureEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kSecureEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::8").value(),
                 /*port=*/0)};
  auto secure_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kSecureEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kOldSecureEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::7").value(),
                 /*port=*/0)};
  auto old_secure_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kOldSecureEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;

  // Add in the secure results first to ensure they're not being selected by
  // being the most recently added result.
  cache.Set(std::move(old_secure_result), anonymization_key,
            HostResolverSource::SYSTEM,
            /*secure=*/true);
  cache.Set(std::move(secure_result), anonymization_key,
            HostResolverSource::DNS,
            /*secure=*/true);
  cache.Set(std::move(insecure_result), anonymization_key,
            HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                   HostResolverSource::ANY, /*secure=*/std::nullopt),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kSecureEndpoints)));

  // Other results still available for more specific lookups.
  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                   HostResolverSource::ANY, /*secure=*/false),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kInsecureEndpoints)));
  EXPECT_THAT(
      cache.Lookup(kName, anonymization_key, DnsQueryType::AAAA,
                   HostResolverSource::SYSTEM, /*secure=*/std::nullopt),
      Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + kTtl),
          Optional(clock_.Now() + kTtl), kOldSecureEndpoints)));
}

// Even though LookupStale() can return stale results, if a wildcard lookup
// matches multiple result entries, expect the lookup to prefer a non-stale
// result.
TEST_F(HostResolverCacheTest, LookupStalePrefersNonStaleResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kStaleEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::7").value(),
                 /*port=*/0)};
  auto stale_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Seconds(4),
      clock_.Now() - base::Seconds(4), HostResolverInternalResult::Source::kDns,
      kStaleEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kActiveEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::8").value(),
                 /*port=*/0)};
  auto active_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(3),
      clock_.Now() + base::Minutes(3), HostResolverInternalResult::Source::kDns,
      kActiveEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;

  cache.Set(std::move(active_result), anonymization_key,
            HostResolverSource::DNS,
            /*secure=*/false);
  cache.Set(std::move(stale_result), anonymization_key,
            HostResolverSource::SYSTEM,
            /*secure=*/true);

  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::ANY, /*secure=*/std::nullopt),
      Optional(IsNotStale(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + base::Minutes(3)),
          Optional(clock_.Now() + base::Minutes(3)), kActiveEndpoints))));

  // Other result still available for more specific lookups.
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::SYSTEM, /*secure=*/std::nullopt),
      Optional(IsStale(
          ExpectHostResolverInternalDataResult(
              kName, DnsQueryType::AAAA,
              HostResolverInternalResult::Source::kDns,
              Optional(tick_clock_.NowTicks() - base::Seconds(4)),
              Optional(clock_.Now() - base::Seconds(4)), kStaleEndpoints),
          Ne(std::nullopt), false)));
}

// Same as LookupStalePrefersNonStaleResult except lookup criteria specifies
// insecure. Expect same general behavior (prefers non-stale result) but
// exercises slightly different logic because, if no secure results exist, no
// other results need to be considered once a non-stale result is found
TEST_F(HostResolverCacheTest, InsecureLookupStalePrefersNonStaleResult) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kStaleEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::7").value(),
                 /*port=*/0)};
  auto stale_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Seconds(4),
      clock_.Now() - base::Seconds(4), HostResolverInternalResult::Source::kDns,
      kStaleEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kActiveEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::8").value(),
                 /*port=*/0)};
  auto active_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(3),
      clock_.Now() + base::Minutes(3), HostResolverInternalResult::Source::kDns,
      kActiveEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;

  cache.Set(std::move(stale_result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);
  cache.Set(std::move(active_result), anonymization_key,
            HostResolverSource::SYSTEM,
            /*secure=*/false);

  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::ANY, /*secure=*/false),
      Optional(IsNotStale(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(tick_clock_.NowTicks() + base::Minutes(3)),
          Optional(clock_.Now() + base::Minutes(3)), kActiveEndpoints))));
}

TEST_F(HostResolverCacheTest, LookupStalePrefersLeastStaleByGeneration) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kMoreStaleEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::7").value(),
                 /*port=*/0)};
  auto more_stale_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Seconds(4),
      clock_.Now() + base::Seconds(4), HostResolverInternalResult::Source::kDns,
      kMoreStaleEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kLessStaleEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::8").value(),
                 /*port=*/0)};
  auto less_stale_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Minutes(3),
      clock_.Now() - base::Minutes(3), HostResolverInternalResult::Source::kDns,
      kLessStaleEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;

  cache.Set(std::move(more_stale_result), anonymization_key,
            HostResolverSource::DNS,
            /*secure=*/true);
  cache.MakeAllResultsStale();
  cache.Set(std::move(less_stale_result), anonymization_key,
            HostResolverSource::SYSTEM,
            /*secure=*/false);

  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::ANY, /*secure=*/std::nullopt),
      Optional(IsStale(
          ExpectHostResolverInternalDataResult(
              kName, DnsQueryType::AAAA,
              HostResolverInternalResult::Source::kDns,
              Optional(tick_clock_.NowTicks() - base::Minutes(3)),
              Optional(clock_.Now() - base::Minutes(3)), kLessStaleEndpoints),
          Ne(std::nullopt), false)));

  // Other result still available for more specific lookups.
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::DNS, /*secure=*/std::nullopt),
      Optional(IsStale(
          ExpectHostResolverInternalDataResult(
              kName, DnsQueryType::AAAA,
              HostResolverInternalResult::Source::kDns,
              Optional(tick_clock_.NowTicks() + base::Seconds(4)),
              Optional(clock_.Now() + base::Seconds(4)), kMoreStaleEndpoints),
          std::nullopt, true)));
}

TEST_F(HostResolverCacheTest, LookupStalePrefersLeastStaleByExpiration) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kLessStaleEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::8").value(),
                 /*port=*/0)};
  auto less_stale_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Minutes(3),
      clock_.Now() - base::Minutes(3), HostResolverInternalResult::Source::kDns,
      kLessStaleEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kMoreStaleEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::7").value(),
                 /*port=*/0)};
  auto more_stale_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Hours(1),
      clock_.Now() - base::Hours(1), HostResolverInternalResult::Source::kDns,
      kMoreStaleEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;

  cache.Set(std::move(less_stale_result), anonymization_key,
            HostResolverSource::SYSTEM,
            /*secure=*/false);
  cache.Set(std::move(more_stale_result), anonymization_key,
            HostResolverSource::DNS,
            /*secure=*/true);

  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::ANY, /*secure=*/std::nullopt),
      Optional(IsStale(
          ExpectHostResolverInternalDataResult(
              kName, DnsQueryType::AAAA,
              HostResolverInternalResult::Source::kDns, Ne(std::nullopt),
              Ne(std::nullopt), kLessStaleEndpoints),
          Optional(TimeDeltaIsApproximately(base::Minutes(3))), false)));

  // Other result still available for more specific lookups.
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::DNS, /*secure=*/std::nullopt),
      Optional(
          IsStale(ExpectHostResolverInternalDataResult(
                      kName, DnsQueryType::AAAA,
                      HostResolverInternalResult::Source::kDns,
                      Ne(std::nullopt), Ne(std::nullopt), kMoreStaleEndpoints),
                  Optional(TimeDeltaIsApproximately(base::Hours(1))), false)));
}

TEST_F(HostResolverCacheTest, LookupStalePrefersMostSecure) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kSecureEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::8").value(),
                 /*port=*/0)};
  auto secure_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Minutes(3),
      clock_.Now() - base::Minutes(3), HostResolverInternalResult::Source::kDns,
      kSecureEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kInsecureEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::7").value(),
                 /*port=*/0)};
  auto insecure_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Minutes(3),
      clock_.Now() - base::Minutes(3), HostResolverInternalResult::Source::kDns,
      kInsecureEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;

  cache.Set(std::move(secure_result), anonymization_key,
            HostResolverSource::SYSTEM,
            /*secure=*/true);
  cache.Set(std::move(insecure_result), anonymization_key,
            HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::ANY, /*secure=*/std::nullopt),
      Optional(
          IsStale(ExpectHostResolverInternalDataResult(
                      kName, DnsQueryType::AAAA,
                      HostResolverInternalResult::Source::kDns,
                      Ne(std::nullopt), Ne(std::nullopt), kSecureEndpoints),
                  Ne(std::nullopt), false)));

  // Other result still available for more specific lookups.
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::DNS, /*secure=*/std::nullopt),
      Optional(
          IsStale(ExpectHostResolverInternalDataResult(
                      kName, DnsQueryType::AAAA,
                      HostResolverInternalResult::Source::kDns,
                      Ne(std::nullopt), Ne(std::nullopt), kInsecureEndpoints),
                  Ne(std::nullopt), false)));
}

// Same as LookupStalePrefersMostSecure except results are not stale. Expect
// same general behavior (secure result preferred) but exercises slightly
// different logic because no other results need to be considered once a
// non-stale secure result is found.
TEST_F(HostResolverCacheTest, LookupStalePrefersMostSecureNonStale) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kInsecureEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::7").value(),
                 /*port=*/0)};
  auto insecure_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(3),
      clock_.Now() + base::Minutes(3), HostResolverInternalResult::Source::kDns,
      kInsecureEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kSecureEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::8").value(),
                 /*port=*/0)};
  auto secure_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(3),
      clock_.Now() + base::Minutes(3), HostResolverInternalResult::Source::kDns,
      kSecureEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;

  cache.Set(std::move(insecure_result), anonymization_key,
            HostResolverSource::DNS,
            /*secure=*/false);
  cache.Set(std::move(secure_result), anonymization_key,
            HostResolverSource::SYSTEM,
            /*secure=*/true);

  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::ANY, /*secure=*/std::nullopt),
      Optional(IsNotStale(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Ne(std::nullopt), Ne(std::nullopt), kSecureEndpoints))));
}

TEST_F(HostResolverCacheTest, LookupStalePrefersMoreRecent) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);

  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kOldEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::8").value(),
                 /*port=*/0)};
  auto old_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Minutes(3),
      clock_.Now() - base::Minutes(3), HostResolverInternalResult::Source::kDns,
      kOldEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const std::vector<IPEndPoint> kNewEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::7").value(),
                 /*port=*/0)};
  auto new_result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Minutes(3),
      clock_.Now() - base::Minutes(3), HostResolverInternalResult::Source::kDns,
      kNewEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;

  cache.Set(std::move(old_result), anonymization_key,
            HostResolverSource::SYSTEM,
            /*secure=*/false);
  cache.Set(std::move(new_result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::ANY, /*secure=*/std::nullopt),
      Optional(IsStale(ExpectHostResolverInternalDataResult(
                           kName, DnsQueryType::AAAA,
                           HostResolverInternalResult::Source::kDns,
                           Ne(std::nullopt), Ne(std::nullopt), kNewEndpoints),
                       Ne(std::nullopt), false)));

  // Other result still available for more specific lookups.
  EXPECT_THAT(
      cache.LookupStale(kName, anonymization_key, DnsQueryType::AAAA,
                        HostResolverSource::SYSTEM, /*secure=*/std::nullopt),
      Optional(IsStale(ExpectHostResolverInternalDataResult(
                           kName, DnsQueryType::AAAA,
                           HostResolverInternalResult::Source::kDns,
                           Ne(std::nullopt), Ne(std::nullopt), kOldEndpoints),
                       Ne(std::nullopt), false)));
}

TEST_F(HostResolverCacheTest, EvictStaleResults) {
  HostResolverCache cache(/*max_results=*/2, clock_, tick_clock_);

  const std::string kName1 = "foo1.test";
  const std::vector<IPEndPoint> kEndpoints1 = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result1 = std::make_unique<HostResolverInternalDataResult>(
      kName1, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(11),
      clock_.Now() + base::Minutes(11),
      HostResolverInternalResult::Source::kDns, kEndpoints1,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);
  cache.MakeAllResultsStale();

  const std::string kName2 = "foo2.test";
  const std::vector<IPEndPoint> kEndpoints2 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto result2 = std::make_unique<HostResolverInternalDataResult>(
      kName2, DnsQueryType::AAAA, tick_clock_.NowTicks() - base::Minutes(4),
      clock_.Now() - base::Minutes(4), HostResolverInternalResult::Source::kDns,
      kEndpoints2,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect `result1` to be stale via generation and `result2` to be stale via
  // expiration.
  EXPECT_THAT(cache.LookupStale(kName1, anonymization_key),
              Optional(IsStale(std::nullopt, true)));
  EXPECT_THAT(cache.LookupStale(kName2, anonymization_key),
              Optional(IsStale(Ne(std::nullopt), false)));

  const std::string kName3 = "foo3.test";
  const std::vector<IPEndPoint> kEndpoints3 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::5").value(),
                 /*port=*/0)};
  auto result3 = std::make_unique<HostResolverInternalDataResult>(
      kName3, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(8),
      clock_.Now() + base::Minutes(8), HostResolverInternalResult::Source::kDns,
      kEndpoints3,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result3), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect `result1` and `result2` to be evicted and `result3` to still be
  // active.
  EXPECT_EQ(cache.LookupStale(kName1, anonymization_key), std::nullopt);
  EXPECT_EQ(cache.LookupStale(kName2, anonymization_key), std::nullopt);
  EXPECT_NE(cache.Lookup(kName3, anonymization_key), nullptr);
}

TEST_F(HostResolverCacheTest, EvictSoonestToExpireResult) {
  HostResolverCache cache(/*max_results=*/2, clock_, tick_clock_);

  const std::string kName1 = "foo1.test";
  const std::vector<IPEndPoint> kEndpoints1 = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result1 = std::make_unique<HostResolverInternalDataResult>(
      kName1, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(11),
      clock_.Now() + base::Minutes(11),
      HostResolverInternalResult::Source::kDns, kEndpoints1,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  const std::string kName2 = "foo2.test";
  const std::vector<IPEndPoint> kEndpoints2 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto result2 = std::make_unique<HostResolverInternalDataResult>(
      kName2, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(4),
      clock_.Now() + base::Minutes(4), HostResolverInternalResult::Source::kDns,
      kEndpoints2,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect both results to be active.
  EXPECT_NE(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_NE(cache.Lookup(kName2, anonymization_key), nullptr);

  const std::string kName3 = "foo3.test";
  const std::vector<IPEndPoint> kEndpoints3 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::5").value(),
                 /*port=*/0)};
  auto result3 = std::make_unique<HostResolverInternalDataResult>(
      kName3, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(8),
      clock_.Now() + base::Minutes(8), HostResolverInternalResult::Source::kDns,
      kEndpoints3,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result3), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect `result2` to be evicted because it expires soonest.
  EXPECT_NE(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_EQ(cache.LookupStale(kName2, anonymization_key), std::nullopt);
  EXPECT_NE(cache.Lookup(kName3, anonymization_key), nullptr);
}

// If multiple results are equally soon-to-expire, expect least secure option to
// be evicted.
TEST_F(HostResolverCacheTest, EvictLeastSecureResult) {
  HostResolverCache cache(/*max_results=*/2, clock_, tick_clock_);

  const std::string kName1 = "foo1.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kEndpoints1 = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result1 = std::make_unique<HostResolverInternalDataResult>(
      kName1, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints1,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/true);

  const std::string kName2 = "foo2.test";
  const std::vector<IPEndPoint> kEndpoints2 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto result2 = std::make_unique<HostResolverInternalDataResult>(
      kName2, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints2,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect both results to be active.
  EXPECT_NE(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_NE(cache.Lookup(kName2, anonymization_key), nullptr);

  const std::string kName3 = "foo3.test";
  const std::vector<IPEndPoint> kEndpoints3 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::5").value(),
                 /*port=*/0)};
  auto result3 = std::make_unique<HostResolverInternalDataResult>(
      kName3, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(8),
      clock_.Now() + base::Minutes(8), HostResolverInternalResult::Source::kDns,
      kEndpoints3,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result3), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect `result2` to be evicted because, while it will expire at the same
  // time as `result1`, it is less secure.
  EXPECT_NE(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_EQ(cache.LookupStale(kName2, anonymization_key), std::nullopt);
  EXPECT_NE(cache.Lookup(kName3, anonymization_key), nullptr);
}

// If multiple results are equally soon-to-expire and equally (in)secure, expect
// oldest option to be evicted.
TEST_F(HostResolverCacheTest, EvictOldestResult) {
  HostResolverCache cache(/*max_results=*/2, clock_, tick_clock_);

  const std::string kName1 = "foo1.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kEndpoints1 = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result1 = std::make_unique<HostResolverInternalDataResult>(
      kName1, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints1,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  const std::string kName2 = "foo2.test";
  const std::vector<IPEndPoint> kEndpoints2 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto result2 = std::make_unique<HostResolverInternalDataResult>(
      kName2, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints2,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect both results to be active.
  EXPECT_NE(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_NE(cache.Lookup(kName2, anonymization_key), nullptr);

  const std::string kName3 = "foo3.test";
  const std::vector<IPEndPoint> kEndpoints3 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::5").value(),
                 /*port=*/0)};
  auto result3 = std::make_unique<HostResolverInternalDataResult>(
      kName3, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(8),
      clock_.Now() + base::Minutes(8), HostResolverInternalResult::Source::kDns,
      kEndpoints3,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result3), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect `result1` to be evicted because, while it will expire at the same
  // time as `result2` and both are insecure, it is older.
  EXPECT_EQ(cache.LookupStale(kName1, anonymization_key), std::nullopt);
  EXPECT_NE(cache.Lookup(kName2, anonymization_key), nullptr);
  EXPECT_NE(cache.Lookup(kName3, anonymization_key), nullptr);
}

// Even newly-added results that trigger eviction are themselves eligible for
// eviction if best candidate.
TEST_F(HostResolverCacheTest, EvictLatestResult) {
  HostResolverCache cache(/*max_results=*/2, clock_, tick_clock_);

  const std::string kName1 = "foo1.test";
  const base::TimeDelta kTtl = base::Minutes(2);
  const std::vector<IPEndPoint> kEndpoints1 = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  auto result1 = std::make_unique<HostResolverInternalDataResult>(
      kName1, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints1,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});

  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result1), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  const std::string kName2 = "foo2.test";
  const std::vector<IPEndPoint> kEndpoints2 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::4").value(),
                 /*port=*/0)};
  auto result2 = std::make_unique<HostResolverInternalDataResult>(
      kName2, DnsQueryType::AAAA, tick_clock_.NowTicks() + kTtl,
      clock_.Now() + kTtl, HostResolverInternalResult::Source::kDns,
      kEndpoints2,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result2), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect both results to be active.
  EXPECT_NE(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_NE(cache.Lookup(kName2, anonymization_key), nullptr);

  const std::string kName3 = "foo3.test";
  const std::vector<IPEndPoint> kEndpoints3 = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::5").value(),
                 /*port=*/0)};
  auto result3 = std::make_unique<HostResolverInternalDataResult>(
      kName3, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Minutes(1),
      clock_.Now() + base::Minutes(8), HostResolverInternalResult::Source::kDns,
      kEndpoints3,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  cache.Set(std::move(result3), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  // Expect `result3` to be evicted because it is soonest to expire.
  EXPECT_NE(cache.Lookup(kName1, anonymization_key), nullptr);
  EXPECT_NE(cache.Lookup(kName2, anonymization_key), nullptr);
  EXPECT_EQ(cache.LookupStale(kName3, anonymization_key), std::nullopt);
}

TEST_F(HostResolverCacheTest, SerializeAndDeserialize) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);
  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  const base::Time kExpiration = clock_.Now() + base::Hours(2);
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Hours(2),
      kExpiration, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  base::Value value = cache.Serialize();
  EXPECT_EQ(value.GetList().size(), 1u);

  HostResolverCache restored_cache(kMaxResults, clock_, tick_clock_);
  EXPECT_TRUE(restored_cache.RestoreFromValue(value));

  // Expect restored result to be stale by generation.
  EXPECT_THAT(
      restored_cache.LookupStale(kName, anonymization_key),
      Optional(IsStale(ExpectHostResolverInternalDataResult(
                           kName, DnsQueryType::AAAA,
                           HostResolverInternalResult::Source::kDns,
                           Eq(std::nullopt), Optional(kExpiration), kEndpoints),
                       std::nullopt, true)));
}

TEST_F(HostResolverCacheTest, TransientAnonymizationKeyNotSerialized) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);
  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  const base::Time kExpiration = clock_.Now() + base::Hours(2);
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Hours(2),
      kExpiration, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const auto anonymization_key = NetworkAnonymizationKey::CreateTransient();
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  base::Value value = cache.Serialize();
  EXPECT_TRUE(value.GetList().empty());
}

TEST_F(HostResolverCacheTest, DeserializePrefersExistingResults) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);
  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kRestoredEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  const base::Time kExpiration = clock_.Now() + base::Hours(2);
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Hours(2),
      kExpiration, HostResolverInternalResult::Source::kDns, kRestoredEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  base::Value value = cache.Serialize();
  EXPECT_EQ(value.GetList().size(), 1u);

  HostResolverCache restored_cache(kMaxResults, clock_, tick_clock_);

  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::3").value(), /*port=*/0)};
  result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Hours(2),
      kExpiration, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  restored_cache.Set(std::move(result), anonymization_key,
                     HostResolverSource::DNS,
                     /*secure=*/false);

  EXPECT_TRUE(restored_cache.RestoreFromValue(value));

  // Expect pre-restoration result.
  EXPECT_THAT(
      restored_cache.LookupStale(kName, anonymization_key),
      Optional(IsNotStale(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Ne(std::nullopt), Optional(kExpiration), kEndpoints))));
}

TEST_F(HostResolverCacheTest, DeserializeStopsBeforeEviction) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);
  const std::string kName1 = "foo1.test";
  const std::vector<IPEndPoint> kRestoredEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  const base::Time kExpiration = clock_.Now() + base::Hours(2);
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName1, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Hours(2),
      kExpiration, HostResolverInternalResult::Source::kDns, kRestoredEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  base::Value value = cache.Serialize();
  EXPECT_EQ(value.GetList().size(), 1u);

  HostResolverCache restored_cache(1, clock_, tick_clock_);

  const std::string kName2 = "foo2.test";
  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("2001:DB8::3").value(), /*port=*/0)};
  result = std::make_unique<HostResolverInternalDataResult>(
      kName2, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Hours(2),
      kExpiration, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  restored_cache.Set(std::move(result), anonymization_key,
                     HostResolverSource::DNS,
                     /*secure=*/false);

  EXPECT_TRUE(restored_cache.RestoreFromValue(value));

  // Expect only pre-restoration result.
  EXPECT_EQ(restored_cache.LookupStale(kName1, anonymization_key),
            std::nullopt);
  EXPECT_THAT(
      restored_cache.LookupStale(kName2, anonymization_key),
      Optional(IsNotStale(ExpectHostResolverInternalDataResult(
          kName2, DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Ne(std::nullopt), Optional(kExpiration), kEndpoints))));
}

TEST_F(HostResolverCacheTest, SerializeForLogging) {
  HostResolverCache cache(kMaxResults, clock_, tick_clock_);
  const std::string kName = "foo.test";
  const std::vector<IPEndPoint> kEndpoints = {
      IPEndPoint(IPAddress::FromIPLiteral("::1").value(), /*port=*/0)};
  const base::Time kExpiration = clock_.Now() + base::Hours(2);
  auto result = std::make_unique<HostResolverInternalDataResult>(
      kName, DnsQueryType::AAAA, tick_clock_.NowTicks() + base::Hours(2),
      kExpiration, HostResolverInternalResult::Source::kDns, kEndpoints,
      /*strings=*/std::vector<std::string>{},
      /*hosts=*/std::vector<HostPortPair>{});
  const NetworkAnonymizationKey anonymization_key;
  cache.Set(std::move(result), anonymization_key, HostResolverSource::DNS,
            /*secure=*/false);

  base::Value value = cache.SerializeForLogging();
  EXPECT_TRUE(value.is_dict());

  EXPECT_FALSE(cache.RestoreFromValue(value));
}

}  // namespace
}  // namespace net
