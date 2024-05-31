// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response_result_extractor.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/base/connection_endpoint_metadata_test_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/host_resolver_internal_result_test_util.h"
#include "net/dns/host_resolver_results_test_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::ResultOf;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

using ExtractionError = DnsResponseResultExtractor::ExtractionError;
using ResultsOrError = DnsResponseResultExtractor::ResultsOrError;

constexpr HostResolverInternalResult::Source kDnsSource =
    HostResolverInternalResult::Source::kDns;

class DnsResponseResultExtractorTest : public ::testing::Test {
 protected:
  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
};

TEST_F(DnsResponseResultExtractorTest, ExtractsSingleARecord) {
  constexpr char kName[] = "address.test";
  const IPAddress kExpected(192, 168, 0, 1);

  DnsResponse response = BuildTestDnsAddressResponse(kName, kExpected);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
                  kName, DnsQueryType::A, kDnsSource,
                  /*expiration_matcher=*/Ne(std::nullopt),
                  /*timed_expiration_matcher=*/Ne(std::nullopt),
                  ElementsAre(IPEndPoint(kExpected, /*port=*/0))))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsSingleAAAARecord) {
  constexpr char kName[] = "address.test";

  IPAddress expected;
  CHECK(expected.AssignFromIPLiteral("2001:4860:4860::8888"));

  DnsResponse response = BuildTestDnsAddressResponse(kName, expected);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::AAAA,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
                  kName, DnsQueryType::AAAA, kDnsSource,
                  /*expiration_matcher=*/Ne(std::nullopt),
                  /*timed_expiration_matcher=*/Ne(std::nullopt),
                  ElementsAre(IPEndPoint(expected, /*port=*/0))))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsSingleARecordWithCname) {
  const IPAddress kExpected(192, 168, 0, 1);
  constexpr char kName[] = "address.test";
  constexpr char kCanonicalName[] = "alias.test";

  DnsResponse response =
      BuildTestDnsAddressResponseWithCname(kName, kExpected, kCanonicalName);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalDataResult(
              kCanonicalName, DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              ElementsAre(IPEndPoint(kExpected, /*port=*/0)))),
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), kCanonicalName))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsARecordsWithCname) {
  constexpr char kName[] = "addresses.test";

  DnsResponse response = BuildTestDnsResponse(
      "addresses.test", dns_protocol::kTypeA,
      {
          BuildTestAddressRecord("alias.test", IPAddress(74, 125, 226, 179)),
          BuildTestAddressRecord("alias.test", IPAddress(74, 125, 226, 180)),
          BuildTestCnameRecord(kName, "alias.test"),
          BuildTestAddressRecord("alias.test", IPAddress(74, 125, 226, 176)),
          BuildTestAddressRecord("alias.test", IPAddress(74, 125, 226, 177)),
          BuildTestAddressRecord("alias.test", IPAddress(74, 125, 226, 178)),
      });
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalDataResult(
              "alias.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              UnorderedElementsAre(
                  IPEndPoint(IPAddress(74, 125, 226, 179), /*port=*/0),
                  IPEndPoint(IPAddress(74, 125, 226, 180), /*port=*/0),
                  IPEndPoint(IPAddress(74, 125, 226, 176), /*port=*/0),
                  IPEndPoint(IPAddress(74, 125, 226, 177), /*port=*/0),
                  IPEndPoint(IPAddress(74, 125, 226, 178), /*port=*/0)))),
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "alias.test"))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsNxdomainAResponses) {
  constexpr char kName[] = "address.test";
  constexpr auto kTtl = base::Hours(2);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)},
      /*additional=*/{}, dns_protocol::kRcodeNXDOMAIN);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalErrorResult(
                  kName, DnsQueryType::A, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ERR_NAME_NOT_RESOLVED))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsNodataAResponses) {
  constexpr char kName[] = "address.test";
  constexpr auto kTtl = base::Minutes(15);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalErrorResult(
                  kName, DnsQueryType::A, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ERR_NAME_NOT_RESOLVED))));
}

TEST_F(DnsResponseResultExtractorTest, RejectsMalformedARecord) {
  constexpr char kName[] = "address.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA,
      {BuildTestDnsRecord(kName, dns_protocol::kTypeA,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::A,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kMalformedRecord);
}

TEST_F(DnsResponseResultExtractorTest, RejectsWrongNameARecord) {
  constexpr char kName[] = "address.test";

  DnsResponse response = BuildTestDnsAddressResponse(
      kName, IPAddress(1, 2, 3, 4), "different.test");
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::A,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kNameMismatch);
}

TEST_F(DnsResponseResultExtractorTest, IgnoresWrongTypeRecordsInAResponse) {
  constexpr char kName[] = "address.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA,
      {BuildTestTextRecord("address.test", {"foo"} /* text_strings */)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  // Expect empty results because NODATA is not cacheable (due to no TTL).
  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(), IsEmpty());
}

TEST_F(DnsResponseResultExtractorTest,
       IgnoresWrongTypeRecordsMixedWithARecords) {
  constexpr char kName[] = "address.test";
  const IPAddress kExpected(8, 8, 8, 8);
  constexpr auto kTtl = base::Days(3);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA,
      {BuildTestTextRecord(kName, /*text_strings=*/{"foo"}, base::Hours(2)),
       BuildTestAddressRecord(kName, kExpected, kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
                  kName, DnsQueryType::A, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ElementsAre(IPEndPoint(kExpected, /*port=*/0))))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsMinATtl) {
  constexpr char kName[] = "name.test";
  constexpr base::TimeDelta kMinTtl = base::Minutes(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4), base::Hours(3)),
       BuildTestAddressRecord(kName, IPAddress(2, 3, 4, 5), kMinTtl),
       BuildTestAddressRecord(kName, IPAddress(3, 4, 5, 6),
                              base::Minutes(15))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
                  kName, DnsQueryType::A, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kMinTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kMinTtl),
                  /*endpoints_matcher=*/SizeIs(3)))));
}

MATCHER_P(ContainsContiguousElements, elements, "") {
  return base::ranges::search(arg, elements) != arg.end();
}

TEST_F(DnsResponseResultExtractorTest, ExtractsTxtResponses) {
  constexpr char kName[] = "name.test";

  // Simulate two separate DNS records, each with multiple strings.
  std::vector<std::string> foo_records = {"foo1", "foo2", "foo3"};
  std::vector<std::string> bar_records = {"bar1", "bar2"};
  std::vector<std::vector<std::string>> text_records = {foo_records,
                                                        bar_records};

  DnsResponse response =
      BuildTestDnsTextResponse(kName, std::move(text_records));
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  // Order between separate DNS records is undefined, but each record should
  // stay in order as that order may be meaningful.
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::TXT, kDnsSource,
          /*expiration_matcher=*/Ne(std::nullopt),
          /*timed_expiration_matcher=*/Ne(std::nullopt),
          /*endpoints_matcher=*/IsEmpty(),
          /*strings_matcher=*/
          AllOf(UnorderedElementsAre("foo1", "foo2", "foo3", "bar1", "bar2"),
                ContainsContiguousElements(foo_records),
                ContainsContiguousElements(bar_records))))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsNxdomainTxtResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Days(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)},
      /*additional=*/{}, dns_protocol::kRcodeNXDOMAIN);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalErrorResult(
                  kName, DnsQueryType::TXT, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ERR_NAME_NOT_RESOLVED))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsNodataTxtResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Minutes(42);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      /*answers=*/{}, /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalErrorResult(
                  kName, DnsQueryType::TXT, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ERR_NAME_NOT_RESOLVED))));
}

TEST_F(DnsResponseResultExtractorTest, RejectsMalformedTxtRecord) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestDnsRecord(kName, dns_protocol::kTypeTXT,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::TXT,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kMalformedRecord);
}

TEST_F(DnsResponseResultExtractorTest, RejectsWrongNameTxtRecord) {
  constexpr char kName[] = "name.test";

  DnsResponse response =
      BuildTestDnsTextResponse(kName, {{"foo"}}, "different.test");
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::TXT,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kNameMismatch);
}

TEST_F(DnsResponseResultExtractorTest, IgnoresWrongTypeTxtResponses) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  // Expect empty results because NODATA is not cacheable (due to no TTL).
  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(), IsEmpty());
}

TEST_F(DnsResponseResultExtractorTest, ExtractsMinTxtTtl) {
  constexpr char kName[] = "name.test";
  constexpr base::TimeDelta kMinTtl = base::Minutes(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestTextRecord(kName, {"foo"}, base::Hours(3)),
       BuildTestTextRecord(kName, {"bar"}, kMinTtl),
       BuildTestTextRecord(kName, {"baz"}, base::Minutes(15))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
                  kName, DnsQueryType::TXT, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kMinTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kMinTtl),
                  /*endpoints_matcher=*/IsEmpty(),
                  /*strings_matcher=*/SizeIs(3)))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsPtrResponses) {
  constexpr char kName[] = "name.test";

  DnsResponse response =
      BuildTestDnsPointerResponse(kName, {"foo.com", "bar.com"});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::PTR,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
                  kName, DnsQueryType::PTR, kDnsSource,
                  /*expiration_matcher=*/Ne(std::nullopt),
                  /*timed_expiration_matcher=*/Ne(std::nullopt),
                  /*endpoints_matcher=*/IsEmpty(),
                  /*strings_matcher=*/IsEmpty(),
                  /*hosts_matcher=*/
                  UnorderedElementsAre(HostPortPair("foo.com", 0),
                                       HostPortPair("bar.com", 0))))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsNxdomainPtrResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Hours(5);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypePTR, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)},
      /*additional=*/{}, dns_protocol::kRcodeNXDOMAIN);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::PTR,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalErrorResult(
                  kName, DnsQueryType::PTR, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ERR_NAME_NOT_RESOLVED))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsNodataPtrResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Minutes(50);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypePTR, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::PTR,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalErrorResult(
                  kName, DnsQueryType::PTR, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ERR_NAME_NOT_RESOLVED))));
}

TEST_F(DnsResponseResultExtractorTest, RejectsMalformedPtrRecord) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypePTR,
      {BuildTestDnsRecord(kName, dns_protocol::kTypePTR,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::PTR,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kMalformedRecord);
}

TEST_F(DnsResponseResultExtractorTest, RejectsWrongNamePtrRecord) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsPointerResponse(
      kName, {"foo.com", "bar.com"}, "different.test");
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::PTR,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kNameMismatch);
}

TEST_F(DnsResponseResultExtractorTest, IgnoresWrongTypePtrResponses) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypePTR,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::PTR,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  // Expect empty results because NODATA is not cacheable (due to no TTL).
  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(), IsEmpty());
}

TEST_F(DnsResponseResultExtractorTest, ExtractsSrvResponses) {
  constexpr char kName[] = "name.test";

  const TestServiceRecord kRecord1 = {2, 3, 1223, "foo.com"};
  const TestServiceRecord kRecord2 = {5, 10, 80, "bar.com"};
  const TestServiceRecord kRecord3 = {5, 1, 5, "google.com"};
  const TestServiceRecord kRecord4 = {2, 100, 12345, "chromium.org"};

  DnsResponse response = BuildTestDnsServiceResponse(
      kName, {kRecord1, kRecord2, kRecord3, kRecord4});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::SRV,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
                  kName, DnsQueryType::SRV, kDnsSource,
                  /*expiration_matcher=*/Ne(std::nullopt),
                  /*timed_expiration_matcher=*/Ne(std::nullopt),
                  /*endpoints_matcher=*/IsEmpty(),
                  /*strings_matcher=*/IsEmpty(),
                  /*hosts_matcher=*/
                  UnorderedElementsAre(HostPortPair("foo.com", 1223),
                                       HostPortPair("bar.com", 80),
                                       HostPortPair("google.com", 5),
                                       HostPortPair("chromium.org", 12345))))));

  // Expect ordered by priority, and random within a priority.
  std::vector<HostPortPair> result_hosts =
      (*results.value().begin())->AsData().hosts();
  auto priority2 =
      std::vector<HostPortPair>(result_hosts.begin(), result_hosts.begin() + 2);
  EXPECT_THAT(priority2, testing::UnorderedElementsAre(
                             HostPortPair("foo.com", 1223),
                             HostPortPair("chromium.org", 12345)));
  auto priority5 =
      std::vector<HostPortPair>(result_hosts.begin() + 2, result_hosts.end());
  EXPECT_THAT(priority5,
              testing::UnorderedElementsAre(HostPortPair("bar.com", 80),
                                            HostPortPair("google.com", 5)));
}

// 0-weight services are allowed. Ensure that we can handle such records,
// especially the case where all entries have weight 0.
TEST_F(DnsResponseResultExtractorTest, ExtractsZeroWeightSrvResponses) {
  constexpr char kName[] = "name.test";

  const TestServiceRecord kRecord1 = {5, 0, 80, "bar.com"};
  const TestServiceRecord kRecord2 = {5, 0, 5, "google.com"};

  DnsResponse response =
      BuildTestDnsServiceResponse(kName, {kRecord1, kRecord2});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::SRV,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
                  kName, DnsQueryType::SRV, kDnsSource,
                  /*expiration_matcher=*/Ne(std::nullopt),
                  /*timed_expiration_matcher=*/Ne(std::nullopt),
                  /*endpoints_matcher=*/IsEmpty(),
                  /*strings_matcher=*/IsEmpty(),
                  /*hosts_matcher=*/
                  UnorderedElementsAre(HostPortPair("bar.com", 80),
                                       HostPortPair("google.com", 5))))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsNxdomainSrvResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Days(7);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeSRV, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)},
      /*additional=*/{}, dns_protocol::kRcodeNXDOMAIN);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::SRV,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalErrorResult(
                  kName, DnsQueryType::SRV, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ERR_NAME_NOT_RESOLVED))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsNodataSrvResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Hours(12);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeSRV, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::SRV,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalErrorResult(
                  kName, DnsQueryType::SRV, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ERR_NAME_NOT_RESOLVED))));
}

TEST_F(DnsResponseResultExtractorTest, RejectsMalformedSrvRecord) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeSRV,
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSRV,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::SRV,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kMalformedRecord);
}

TEST_F(DnsResponseResultExtractorTest, RejectsWrongNameSrvRecord) {
  constexpr char kName[] = "name.test";

  const TestServiceRecord kRecord = {2, 3, 1223, "foo.com"};
  DnsResponse response =
      BuildTestDnsServiceResponse(kName, {kRecord}, "different.test");
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::SRV,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kNameMismatch);
}

TEST_F(DnsResponseResultExtractorTest, IgnoresWrongTypeSrvResponses) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeSRV,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::SRV,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  // Expect empty results because NODATA is not cacheable (due to no TTL).
  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(), IsEmpty());
}

TEST_F(DnsResponseResultExtractorTest, ExtractsBasicHttpsResponses) {
  constexpr char kName[] = "https.test";
  constexpr auto kTtl = base::Hours(12);

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeHttps,
                           {BuildTestHttpsServiceRecord(kName,
                                                        /*priority=*/4,
                                                        /*service_name=*/".",
                                                        /*params=*/{}, kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          Eq(tick_clock_.NowTicks() + kTtl), Eq(clock_.Now() + kTtl),
          ElementsAre(
              Pair(4, ExpectConnectionEndpointMetadata(
                          ElementsAre(dns_protocol::kHttpsServiceDefaultAlpn),
                          /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsComprehensiveHttpsResponses) {
  constexpr char kName[] = "https.test";
  constexpr char kAlpn[] = "foo";
  constexpr uint8_t kEchConfig[] = "EEEEEEEEECH!";
  constexpr auto kTtl = base::Hours(12);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({kAlpn}),
            BuildTestHttpsServiceEchConfigParam(kEchConfig)},
           kTtl),
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/3,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({kAlpn}),
            {dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}},
           /*ttl=*/base::Days(3))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          Eq(tick_clock_.NowTicks() + kTtl), Eq(clock_.Now() + kTtl),
          ElementsAre(
              Pair(3, ExpectConnectionEndpointMetadata(
                          ElementsAre(kAlpn),
                          /*ech_config_list_matcher=*/IsEmpty(), kName)),
              Pair(4, ExpectConnectionEndpointMetadata(
                          ElementsAre(kAlpn,
                                      dns_protocol::kHttpsServiceDefaultAlpn),
                          ElementsAreArray(kEchConfig), kName)))))));
}

TEST_F(DnsResponseResultExtractorTest, IgnoresHttpsResponseWithJustAlias) {
  constexpr char kName[] = "https.test";
  constexpr base::TimeDelta kTtl = base::Days(5);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsAliasRecord(kName, "alias.test", kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  // Expect empty metadata result to signify compatible HTTPS records with no
  // data of use to Chrome. Still expect expiration from record, so the empty
  // response can be cached.
  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Optional(tick_clock_.NowTicks() + kTtl),
          /*timed_expiration_matcher=*/Optional(clock_.Now() + kTtl),
          /*metadatas_matcher=*/IsEmpty()))));
}

TEST_F(DnsResponseResultExtractorTest, IgnoresHttpsResponseWithAlias) {
  constexpr char kName[] = "https.test";
  constexpr base::TimeDelta kLowestTtl = base::Minutes(32);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName,
                                   /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/{}, base::Days(1)),
       BuildTestHttpsAliasRecord(kName, "alias.test", kLowestTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  // Expect empty metadata result to signify compatible HTTPS records with no
  // data of use to Chrome. Expiration should match lowest TTL from all
  // compatible records.
  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Optional(tick_clock_.NowTicks() + kLowestTtl),
          /*timed_expiration_matcher=*/Optional(clock_.Now() + kLowestTtl),
          /*metadatas_matcher=*/IsEmpty()))));
}

// Expect the entire response to be ignored if all HTTPS records have the
// "no-default-alpn" param.
TEST_F(DnsResponseResultExtractorTest, IgnoresHttpsResponseWithNoDefaultAlpn) {
  constexpr char kName[] = "https.test";
  constexpr base::TimeDelta kLowestTtl = base::Hours(3);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"foo1"}),
            {dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}},
           kLowestTtl),
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/5,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"foo2"}),
            {dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}},
           base::Days(3))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  // Expect empty metadata result to signify compatible HTTPS records with no
  // data of use to Chrome.
  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Optional(tick_clock_.NowTicks() + kLowestTtl),
          /*timed_expiration_matcher=*/Optional(clock_.Now() + kLowestTtl),
          /*metadatas_matcher=*/IsEmpty()))));
}

// Unsupported/unknown HTTPS params are simply ignored if not marked mandatory.
TEST_F(DnsResponseResultExtractorTest, IgnoresUnsupportedParamsInHttpsRecord) {
  constexpr char kName[] = "https.test";
  constexpr uint16_t kMadeUpParamKey = 65500;  // From the private-use block.

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {{kMadeUpParamKey, "foo"}})});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Ne(std::nullopt),
          /*timed_expiration_matcher=*/Ne(std::nullopt),
          ElementsAre(
              Pair(4, ExpectConnectionEndpointMetadata(
                          ElementsAre(dns_protocol::kHttpsServiceDefaultAlpn),
                          /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

// Entire record is dropped if an unsupported/unknown HTTPS param is marked
// mandatory.
TEST_F(DnsResponseResultExtractorTest,
       IgnoresHttpsRecordWithUnsupportedMandatoryParam) {
  constexpr char kName[] = "https.test";
  constexpr uint16_t kMadeUpParamKey = 65500;  // From the private-use block.
  constexpr base::TimeDelta kTtl = base::Days(5);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"ignored_alpn"}),
            BuildTestHttpsServiceMandatoryParam({kMadeUpParamKey}),
            {kMadeUpParamKey, "foo"}},
           base::Hours(2)),
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/5,
           /*service_name=*/".",
           /*params=*/{BuildTestHttpsServiceAlpnParam({"foo"})}, kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());

  // Expect expiration to be derived only from non-ignored records.
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Optional(tick_clock_.NowTicks() + kTtl),
          /*timed_expiration_matcher=*/Optional(clock_.Now() + kTtl),
          ElementsAre(Pair(
              5, ExpectConnectionEndpointMetadata(
                     ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
                     /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

TEST_F(DnsResponseResultExtractorTest,
       ExtractsHttpsRecordWithMatchingServiceName) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/kName,
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Ne(std::nullopt),
          /*timed_expiration_matcher=*/Ne(std::nullopt),
          ElementsAre(Pair(
              4, ExpectConnectionEndpointMetadata(
                     ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
                     /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

TEST_F(DnsResponseResultExtractorTest,
       ExtractsHttpsRecordWithMatchingDefaultServiceName) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Ne(std::nullopt),
          /*timed_expiration_matcher=*/Ne(std::nullopt),
          ElementsAre(Pair(
              4, ExpectConnectionEndpointMetadata(
                     ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
                     /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

TEST_F(DnsResponseResultExtractorTest,
       ExtractsHttpsRecordWithPrefixedNameAndMatchingServiceName) {
  constexpr char kName[] = "https.test";
  constexpr char kPrefixedName[] = "_444._https.https.test";

  DnsResponse response = BuildTestDnsResponse(
      kPrefixedName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kPrefixedName, /*priority=*/4,
                                   /*service_name=*/kName,
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kPrefixedName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Ne(std::nullopt),
          /*timed_expiration_matcher=*/Ne(std::nullopt),
          ElementsAre(Pair(
              4, ExpectConnectionEndpointMetadata(
                     ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
                     /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

TEST_F(DnsResponseResultExtractorTest,
       ExtractsHttpsRecordWithAliasingAndMatchingServiceName) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestCnameRecord(kName, "alias.test"),
       BuildTestHttpsServiceRecord("alias.test", /*priority=*/4,
                                   /*service_name=*/kName,
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::HTTPS, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "alias.test")),
          Pointee(ExpectHostResolverInternalMetadataResult(
              "alias.test", DnsQueryType::HTTPS, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              ElementsAre(Pair(
                  4, ExpectConnectionEndpointMetadata(
                         ElementsAre("foo",
                                     dns_protocol::kHttpsServiceDefaultAlpn),
                         /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

TEST_F(DnsResponseResultExtractorTest,
       IgnoreHttpsRecordWithNonMatchingServiceName) {
  constexpr char kName[] = "https.test";
  constexpr base::TimeDelta kTtl = base::Hours(14);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/"other.service.test",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"ignored"})}, base::Hours(3)),
       BuildTestHttpsServiceRecord("https.test", /*priority=*/5,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})},
                                   kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());

  // Expect expiration to be derived only from non-ignored records.
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Optional(tick_clock_.NowTicks() + kTtl),
          /*timed_expiration_matcher=*/Optional(clock_.Now() + kTtl),
          ElementsAre(Pair(
              5, ExpectConnectionEndpointMetadata(
                     ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
                     /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

TEST_F(DnsResponseResultExtractorTest,
       ExtractsHttpsRecordWithPrefixedNameAndDefaultServiceName) {
  constexpr char kPrefixedName[] = "_445._https.https.test";

  DnsResponse response = BuildTestDnsResponse(
      kPrefixedName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kPrefixedName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/"https.test",
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kPrefixedName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Ne(std::nullopt),
          /*timed_expiration_matcher=*/Ne(std::nullopt),
          ElementsAre(Pair(
              4,
              ExpectConnectionEndpointMetadata(
                  ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
                  /*ech_config_list_matcher=*/IsEmpty(), kPrefixedName)))))));
}

TEST_F(DnsResponseResultExtractorTest,
       ExtractsHttpsRecordWithAliasingAndDefaultServiceName) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestCnameRecord(kName, "alias.test"),
       BuildTestHttpsServiceRecord("alias.test", /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::HTTPS, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "alias.test")),
          Pointee(ExpectHostResolverInternalMetadataResult(
              "alias.test", DnsQueryType::HTTPS, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              ElementsAre(Pair(
                  4, ExpectConnectionEndpointMetadata(
                         ElementsAre("foo",
                                     dns_protocol::kHttpsServiceDefaultAlpn),
                         /*ech_config_list_matcher=*/IsEmpty(),
                         "alias.test")))))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsHttpsRecordWithMatchingPort) {
  constexpr char kName[] = "https.test";
  constexpr uint16_t kPort = 4567;

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"}),
                                    BuildTestHttpsServicePortParam(kPort)})});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/kPort);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Ne(std::nullopt),
          /*timed_expiration_matcher=*/Ne(std::nullopt),
          ElementsAre(Pair(
              4, ExpectConnectionEndpointMetadata(
                     ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
                     /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

TEST_F(DnsResponseResultExtractorTest, IgnoresHttpsRecordWithMismatchingPort) {
  constexpr char kName[] = "https.test";
  constexpr base::TimeDelta kTtl = base::Days(14);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"ignored"}),
                                    BuildTestHttpsServicePortParam(1003)},
                                   base::Hours(12)),
       BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})},
                                   kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/55);

  ASSERT_TRUE(results.has_value());

  // Expect expiration to be derived only from non-ignored records.
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Optional(tick_clock_.NowTicks() + kTtl),
          /*timed_expiration_matcher=*/Optional(clock_.Now() + kTtl),
          ElementsAre(Pair(
              4, ExpectConnectionEndpointMetadata(
                     ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
                     /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

// HTTPS records with "no-default-alpn" but also no "alpn" are not
// "self-consistent" and should be ignored.
TEST_F(DnsResponseResultExtractorTest, IgnoresHttpsRecordWithNoAlpn) {
  constexpr char kName[] = "https.test";
  constexpr base::TimeDelta kTtl = base::Minutes(150);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/".",
           /*params=*/
           {{dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}},
           base::Minutes(10)),
       BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})},
                                   kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/55);

  ASSERT_TRUE(results.has_value());

  // Expect expiration to be derived only from non-ignored records.
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Optional(tick_clock_.NowTicks() + kTtl),
          /*timed_expiration_matcher=*/Optional(clock_.Now() + kTtl),
          ElementsAre(Pair(
              4, ExpectConnectionEndpointMetadata(
                     ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
                     /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

// Expect the entire response to be ignored if all HTTPS records have the
// "no-default-alpn" param.
TEST_F(DnsResponseResultExtractorTest,
       IgnoresHttpsResponseWithNoCompatibleDefaultAlpn) {
  constexpr char kName[] = "https.test";
  constexpr uint16_t kMadeUpParamKey = 65500;  // From the private-use block.
  constexpr base::TimeDelta kLowestTtl = base::Days(2);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"foo1"}),
            {dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}},
           base::Days(3)),
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/5,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"foo2"}),
            {dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}},
           base::Days(4)),
       // Allows default ALPN, but ignored due to non-matching service name.
       BuildTestHttpsServiceRecord(kName, /*priority=*/3,
                                   /*service_name=*/"other.test",
                                   /*params=*/{}, kLowestTtl),
       // Allows default ALPN, but ignored due to incompatible param.
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/6,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceMandatoryParam({kMadeUpParamKey}),
            {kMadeUpParamKey, "foo"}},
           base::Hours(1)),
       // Allows default ALPN, but ignored due to mismatching port.
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/10,
           /*service_name=*/".",
           /*params=*/{BuildTestHttpsServicePortParam(1005)}, base::Days(5))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());

  // Expect expiration to be from the lowest TTL from the "compatible" records
  // that don't have incompatible params.
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          /*expiration_matcher=*/Optional(tick_clock_.NowTicks() + kLowestTtl),
          /*timed_expiration_matcher=*/Optional(clock_.Now() + kLowestTtl),
          /*metadatas_matcher=*/IsEmpty()))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsNxdomainHttpsResponses) {
  constexpr char kName[] = "https.test";
  constexpr auto kTtl = base::Minutes(45);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)},
      /*additional=*/{}, dns_protocol::kRcodeNXDOMAIN);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalErrorResult(
                  kName, DnsQueryType::HTTPS, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ERR_NAME_NOT_RESOLVED))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsNodataHttpsResponses) {
  constexpr char kName[] = "https.test";
  constexpr auto kTtl = base::Hours(36);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalErrorResult(
                  kName, DnsQueryType::HTTPS, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  ERR_NAME_NOT_RESOLVED))));
}

TEST_F(DnsResponseResultExtractorTest, RejectsMalformedHttpsRecord) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestDnsRecord(kName, dns_protocol::kTypeHttps,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::HTTPS,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kMalformedRecord);
}

TEST_F(DnsResponseResultExtractorTest, RejectsWrongNameHttpsRecord) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsAliasRecord("different.test", "alias.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::HTTPS,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kNameMismatch);
}

TEST_F(DnsResponseResultExtractorTest, IgnoresWrongTypeHttpsResponses) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(results.value(), IsEmpty());
}

TEST_F(DnsResponseResultExtractorTest, IgnoresAdditionalHttpsRecords) {
  constexpr char kName[] = "https.test";
  constexpr auto kTtl = base::Days(5);

  // Give all records an "alpn" value to help validate that only the correct
  // record is used.
  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      /*answers=*/
      {BuildTestHttpsServiceRecord(kName, /*priority=*/5u,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo1"})},
                                   kTtl)},
      /*authority=*/{},
      /*additional=*/
      {BuildTestHttpsServiceRecord(kName, /*priority=*/3u,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo2"})},
                                   base::Minutes(44)),
       BuildTestHttpsServiceRecord(kName, /*priority=*/2u,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo3"})},
                                   base::Minutes(30))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(Pointee(ExpectHostResolverInternalMetadataResult(
          kName, DnsQueryType::HTTPS, kDnsSource,
          Eq(tick_clock_.NowTicks() + kTtl), Eq(clock_.Now() + kTtl),
          ElementsAre(Pair(
              5,
              ExpectConnectionEndpointMetadata(
                  ElementsAre("foo1", dns_protocol::kHttpsServiceDefaultAlpn),
                  /*ech_config_list_matcher=*/IsEmpty(), kName)))))));
}

TEST_F(DnsResponseResultExtractorTest, IgnoresUnsolicitedHttpsRecords) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Minutes(45);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      /*answers=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeTXT, "\003foo", kTtl)},
      /*authority=*/{},
      /*additional=*/
      {BuildTestHttpsServiceRecord(
           "https.test", /*priority=*/3u, /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"foo2"})}, base::Minutes(44)),
       BuildTestHttpsServiceRecord("https.test", /*priority=*/2u,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo3"})},
                                   base::Minutes(30))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());

  // Expect expiration to be derived only from the non-ignored answer record.
  EXPECT_THAT(results.value(),
              ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
                  kName, DnsQueryType::TXT, kDnsSource,
                  /*expiration_matcher=*/Eq(tick_clock_.NowTicks() + kTtl),
                  /*timed_expiration_matcher=*/Eq(clock_.Now() + kTtl),
                  /*endpoints_matcher=*/IsEmpty(), ElementsAre("foo")))));
}

TEST_F(DnsResponseResultExtractorTest, HandlesInOrderCnameChain) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord(kName, "second.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestTextRecord("fourth.test", {"bar"})});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "second.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "second.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "third.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "third.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "fourth.test")),
          Pointee(ExpectHostResolverInternalDataResult(
              "fourth.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              /*endpoints_matcher=*/IsEmpty(),
              UnorderedElementsAre("foo", "bar")))));
}

TEST_F(DnsResponseResultExtractorTest, HandlesInOrderCnameChainTypeA) {
  constexpr char kName[] = "first.test";

  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA,
                           {BuildTestCnameRecord(kName, "second.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestAddressRecord("fourth.test", kExpected)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "second.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "second.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "third.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "third.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "fourth.test")),
          Pointee(ExpectHostResolverInternalDataResult(
              "fourth.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              ElementsAre(expected_endpoint)))));
}

TEST_F(DnsResponseResultExtractorTest, HandlesReverseOrderCnameChain) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "second.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "second.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "third.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "third.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "fourth.test")),
          Pointee(ExpectHostResolverInternalDataResult(
              "fourth.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              /*endpoints_matcher=*/IsEmpty(), ElementsAre("foo")))));
}

TEST_F(DnsResponseResultExtractorTest, HandlesReverseOrderCnameChainTypeA) {
  constexpr char kName[] = "first.test";

  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA,
                           {BuildTestAddressRecord("fourth.test", kExpected),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "second.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "second.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "third.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "third.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "fourth.test")),
          Pointee(ExpectHostResolverInternalDataResult(
              "fourth.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              ElementsAre(expected_endpoint)))));
}

TEST_F(DnsResponseResultExtractorTest, HandlesArbitraryOrderCnameChain) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "second.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "second.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "third.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "third.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "fourth.test")),
          Pointee(ExpectHostResolverInternalDataResult(
              "fourth.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              /*endpoints_matcher=*/IsEmpty(), ElementsAre("foo")))));
}

TEST_F(DnsResponseResultExtractorTest, HandlesArbitraryOrderCnameChainTypeA) {
  constexpr char kName[] = "first.test";

  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  // Alias names are chosen so that the chain order is not in alphabetical
  // order.
  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA,
                           {BuildTestCnameRecord("qsecond.test", "athird.test"),
                            BuildTestAddressRecord("zfourth.test", kExpected),
                            BuildTestCnameRecord("athird.test", "zfourth.test"),
                            BuildTestCnameRecord(kName, "qsecond.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "qsecond.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "qsecond.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "athird.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "athird.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "zfourth.test")),
          Pointee(ExpectHostResolverInternalDataResult(
              "zfourth.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              ElementsAre(expected_endpoint)))));
}

TEST_F(DnsResponseResultExtractorTest,
       IgnoresNonResultTypesMixedWithCnameChain) {
  constexpr char kName[] = "first.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestCnameRecord("second.test", "third.test"),
       BuildTestTextRecord("fourth.test", {"foo"}),
       BuildTestCnameRecord("third.test", "fourth.test"),
       BuildTestAddressRecord("third.test", IPAddress(1, 2, 3, 4)),
       BuildTestCnameRecord(kName, "second.test"),
       BuildTestAddressRecord("fourth.test", IPAddress(2, 3, 4, 5))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "second.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "second.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "third.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "third.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "fourth.test")),
          Pointee(ExpectHostResolverInternalDataResult(
              "fourth.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              /*endpoints_matcher=*/IsEmpty(), ElementsAre("foo")))));
}

TEST_F(DnsResponseResultExtractorTest,
       IgnoresNonResultTypesMixedWithCnameChainTypeA) {
  constexpr char kName[] = "first.test";

  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test"),
                            BuildTestAddressRecord("fourth.test", kExpected)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "second.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "second.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "third.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "third.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "fourth.test")),
          Pointee(ExpectHostResolverInternalDataResult(
              "fourth.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              ElementsAre(expected_endpoint)))));
}

TEST_F(DnsResponseResultExtractorTest, HandlesCnameChainWithoutResult) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "second.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "second.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "third.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "third.test", DnsQueryType::TXT, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "fourth.test"))));
}

TEST_F(DnsResponseResultExtractorTest, HandlesCnameChainWithoutResultTypeA) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "second.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "second.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "third.test")),
          Pointee(ExpectHostResolverInternalAliasResult(
              "third.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "fourth.test"))));
}

TEST_F(DnsResponseResultExtractorTest, RejectsCnameChainWithLoop) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("third.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "second.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::TXT,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kBadAliasChain);
}

TEST_F(DnsResponseResultExtractorTest, RejectsCnameChainWithLoopToBeginning) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("third.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "first.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::TXT,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kBadAliasChain);
}

TEST_F(DnsResponseResultExtractorTest,
       RejectsCnameChainWithLoopToBeginningWithoutResult) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "first.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::TXT,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kBadAliasChain);
}

TEST_F(DnsResponseResultExtractorTest, RejectsCnameChainWithWrongStart) {
  constexpr char kName[] = "test.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::TXT,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kBadAliasChain);
}

TEST_F(DnsResponseResultExtractorTest, RejectsCnameChainWithWrongResultName) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("third.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::TXT,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kNameMismatch);
}

TEST_F(DnsResponseResultExtractorTest, RejectsCnameSharedWithResult) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord(kName, {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::TXT,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kNameMismatch);
}

TEST_F(DnsResponseResultExtractorTest, RejectsDisjointCnameChain) {
  constexpr char kName[] = "first.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestCnameRecord("second.test", "third.test"),
       BuildTestTextRecord("fourth.test", {"foo"}),
       BuildTestCnameRecord("third.test", "fourth.test"),
       BuildTestCnameRecord("other1.test", "other2.test"),
       BuildTestCnameRecord(kName, "second.test"),
       BuildTestCnameRecord("other2.test", "other3.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::TXT,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kBadAliasChain);
}

TEST_F(DnsResponseResultExtractorTest, RejectsDoubledCnames) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("third.test", "fifth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::TXT,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kMultipleCnames);
}

TEST_F(DnsResponseResultExtractorTest, IgnoresTtlFromNonResultType) {
  constexpr char kName[] = "name.test";
  constexpr base::TimeDelta kMinTtl = base::Minutes(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestTextRecord(kName, {"foo"}, base::Hours(3)),
       BuildTestTextRecord(kName, {"bar"}, kMinTtl),
       BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4), base::Seconds(2)),
       BuildTestTextRecord(kName, {"baz"}, base::Minutes(15))});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      ElementsAre(Pointee(ExpectHostResolverInternalDataResult(
          kName, DnsQueryType::TXT, kDnsSource,
          Eq(tick_clock_.NowTicks() + kMinTtl), Eq(clock_.Now() + kMinTtl),
          /*endpoints_matcher=*/IsEmpty(),
          UnorderedElementsAre("foo", "bar", "baz")))));
}

TEST_F(DnsResponseResultExtractorTest, ExtractsTtlFromCname) {
  constexpr char kName[] = "name.test";
  constexpr char kAlias[] = "alias.test";
  constexpr base::TimeDelta kTtl = base::Minutes(4);

  DnsResponse response =
      BuildTestDnsResponse("name.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord(kName, kAlias, kTtl)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::TXT,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(Pointee(ExpectHostResolverInternalAliasResult(
          kName, DnsQueryType::TXT, kDnsSource,
          Eq(tick_clock_.NowTicks() + kTtl), Eq(clock_.Now() + kTtl),
          kAlias))));
}

TEST_F(DnsResponseResultExtractorTest, ValidatesAliasNames) {
  constexpr char kName[] = "first.test";

  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA,
                           {BuildTestCnameRecord(kName, "second.test"),
                            BuildTestCnameRecord("second.test", "localhost"),
                            BuildTestCnameRecord("localhost", "fourth.test"),
                            BuildTestAddressRecord("fourth.test", kExpected)});
  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  EXPECT_EQ(extractor
                .ExtractDnsResults(DnsQueryType::A,
                                   /*original_domain_name=*/kName,
                                   /*request_port=*/0)
                .error_or(ExtractionError::kOk),
            ExtractionError::kMalformedRecord);
}

TEST_F(DnsResponseResultExtractorTest, CanonicalizesAliasNames) {
  const IPAddress kExpected(192, 168, 0, 1);
  constexpr char kName[] = "address.test";
  constexpr char kCname[] = "\005ALIAS\004test\000";

  // Need to build records directly in order to manually encode alias target
  // name because BuildTestDnsAddressResponseWithCname() uses
  // DNSDomainFromDot() which does not support non-URL-canonicalized names.
  std::vector<DnsResourceRecord> answers = {
      BuildTestDnsRecord(kName, dns_protocol::kTypeCNAME,
                         std::string(kCname, sizeof(kCname) - 1)),
      BuildTestAddressRecord("alias.test", kExpected)};
  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA, answers);

  DnsResponseResultExtractor extractor(response, clock_, tick_clock_);

  ResultsOrError results =
      extractor.ExtractDnsResults(DnsQueryType::A,
                                  /*original_domain_name=*/kName,
                                  /*request_port=*/0);

  ASSERT_TRUE(results.has_value());
  EXPECT_THAT(
      results.value(),
      UnorderedElementsAre(
          Pointee(ExpectHostResolverInternalAliasResult(
              kName, DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt), "alias.test")),
          Pointee(ExpectHostResolverInternalDataResult(
              "alias.test", DnsQueryType::A, kDnsSource,
              /*expiration_matcher=*/Ne(std::nullopt),
              /*timed_expiration_matcher=*/Ne(std::nullopt),
              ElementsAre(IPEndPoint(kExpected, /*port=*/0))))));
}

}  // namespace
}  // namespace net
