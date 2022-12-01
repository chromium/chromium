// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response_result_extractor.h"

#include <string>
#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
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
#include "net/dns/host_resolver_results_test_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
namespace {

TEST(DnsResponseResultExtractorTest, ExtractsSingleARecord) {
  constexpr char kName[] = "address.test";
  const IPAddress kExpected(192, 168, 0, 1);

  DnsResponse response = BuildTestDnsAddressResponse(kName, kExpected);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  EXPECT_THAT(results.GetEndpoints(),
              testing::Optional(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(expected_endpoint)))));
  EXPECT_THAT(results.aliases(), testing::Pointee(testing::ElementsAre(kName)));
  EXPECT_TRUE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, ExtractsSingleAAAARecord) {
  constexpr char kName[] = "address.test";

  IPAddress expected;
  CHECK(expected.AssignFromIPLiteral("2001:4860:4860::8888"));

  DnsResponse response = BuildTestDnsAddressResponse(kName, expected);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::AAAA,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  IPEndPoint expected_endpoint(expected, 0 /* port */);
  EXPECT_THAT(results.GetEndpoints(),
              testing::Optional(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(expected_endpoint)))));
  EXPECT_THAT(results.aliases(), testing::Pointee(testing::ElementsAre(kName)));
}

TEST(DnsResponseResultExtractorTest, ExtractsSingleARecordWithCname) {
  const IPAddress kExpected(192, 168, 0, 1);
  constexpr char kName[] = "address.test";
  constexpr char kCanonicalName[] = "alias.test";

  DnsResponse response =
      BuildTestDnsAddressResponseWithCname(kName, kExpected, kCanonicalName);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);
  EXPECT_THAT(results.GetEndpoints(),
              testing::Optional(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(expected_endpoint)))));
  EXPECT_THAT(results.aliases(), testing::Pointee(testing::UnorderedElementsAre(
                                     kName, kCanonicalName)));
}

TEST(DnsResponseResultExtractorTest, ExtractsARecordsWithCname) {
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
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetEndpoints(),
      testing::Optional(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              IPEndPoint(IPAddress(74, 125, 226, 179), 0 /* port */),
              IPEndPoint(IPAddress(74, 125, 226, 178), 0 /* port */),
              IPEndPoint(IPAddress(74, 125, 226, 180), 0 /* port */),
              IPEndPoint(IPAddress(74, 125, 226, 176), 0 /* port */),
              IPEndPoint(IPAddress(74, 125, 226, 177), 0 /* port */))))));
  EXPECT_THAT(results.aliases(), testing::Pointee(testing::UnorderedElementsAre(
                                     "alias.test", kName)));
}

TEST(DnsResponseResultExtractorTest, ExtractsNxdomainAResponses) {
  constexpr char kName[] = "address.test";
  constexpr auto kTtl = base::Hours(2);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)},
      /*additional=*/{}, dns_protocol::kRcodeNXDOMAIN);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.GetEndpoints(), testing::Optional(testing::IsEmpty()));
  EXPECT_THAT(results.aliases(), testing::Pointee(testing::ElementsAre(kName)));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsNodataAResponses) {
  constexpr char kName[] = "address.test";
  constexpr auto kTtl = base::Minutes(15);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.GetEndpoints(), testing::Optional(testing::IsEmpty()));
  EXPECT_THAT(results.aliases(), testing::Pointee(testing::ElementsAre(kName)));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, RejectsMalformedARecord) {
  constexpr char kName[] = "address.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA,
      {BuildTestDnsRecord(kName, dns_protocol::kTypeA,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kMalformedRecord);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, RejectsWrongNameARecord) {
  constexpr char kName[] = "address.test";

  DnsResponse response = BuildTestDnsAddressResponse(
      kName, IPAddress(1, 2, 3, 4), "different.test");
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypeRecordsInAResponse) {
  constexpr char kName[] = "address.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA,
      {BuildTestTextRecord("address.test", {"foo"} /* text_strings */)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.GetEndpoints(), testing::Optional(testing::IsEmpty()));
  EXPECT_THAT(results.aliases(),
              testing::Pointee(testing::ElementsAre("address.test")));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypeRecordsMixedWithARecords) {
  constexpr char kName[] = "address.test";
  const IPAddress kExpected(8, 8, 8, 8);
  constexpr auto kTtl = base::Days(3);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA,
      {BuildTestTextRecord(kName, /*text_strings=*/{"foo"}, base::Hours(2)),
       BuildTestAddressRecord(kName, kExpected, kTtl)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);
  EXPECT_THAT(results.GetEndpoints(),
              testing::Optional(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(expected_endpoint)))));
  EXPECT_THAT(results.aliases(), testing::Pointee(testing::ElementsAre(kName)));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsMinATtl) {
  constexpr char kName[] = "name.test";
  constexpr base::TimeDelta kMinTtl = base::Minutes(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4), base::Hours(3)),
       BuildTestAddressRecord(kName, IPAddress(2, 3, 4, 5), kMinTtl),
       BuildTestAddressRecord(kName, IPAddress(3, 4, 5, 6),
                              base::Minutes(15))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kMinTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsTxtResponses) {
  constexpr char kName[] = "name.test";

  // Simulate two separate DNS records, each with multiple strings.
  std::vector<std::string> foo_records = {"foo1", "foo2", "foo3"};
  std::vector<std::string> bar_records = {"bar1", "bar2"};
  std::vector<std::vector<std::string>> text_records = {foo_records,
                                                        bar_records};

  DnsResponse response =
      BuildTestDnsTextResponse(kName, std::move(text_records));
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());

  // Order between separate DNS records is undefined, but each record should
  // stay in order as that order may be meaningful.
  ASSERT_THAT(results.text_records(),
              testing::Optional(testing::UnorderedElementsAre(
                  "foo1", "foo2", "foo3", "bar1", "bar2")));
  std::vector<std::string> results_vector = results.text_records().value();
  EXPECT_NE(results_vector.end(),
            base::ranges::search(results_vector, foo_records));
  EXPECT_NE(results_vector.end(),
            base::ranges::search(results_vector, bar_records));
}

TEST(DnsResponseResultExtractorTest, ExtractsNxdomainTxtResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Days(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)},
      /*additional=*/{}, dns_protocol::kRcodeNXDOMAIN);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.text_records(), testing::Optional(testing::IsEmpty()));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsNodataTxtResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Minutes(42);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      /*answers=*/{}, /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.text_records(), testing::Optional(testing::IsEmpty()));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, RejectsMalformedTxtRecord) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestDnsRecord(kName, dns_protocol::kTypeTXT,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kMalformedRecord);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, RejectsWrongNameTxtRecord) {
  constexpr char kName[] = "name.test";

  DnsResponse response =
      BuildTestDnsTextResponse(kName, {{"foo"}}, "different.test");
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypeTxtResponses) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.text_records(), testing::Optional(testing::IsEmpty()));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, ExtractsMinTxtTtl) {
  constexpr char kName[] = "name.test";
  constexpr base::TimeDelta kMinTtl = base::Minutes(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestTextRecord(kName, {"foo"}, base::Hours(3)),
       BuildTestTextRecord(kName, {"bar"}, kMinTtl),
       BuildTestTextRecord(kName, {"baz"}, base::Minutes(15))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kMinTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsPtrResponses) {
  constexpr char kName[] = "name.test";

  DnsResponse response =
      BuildTestDnsPointerResponse(kName, {"foo.com", "bar.com"});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());

  // Order between separate records is undefined.
  EXPECT_THAT(results.hostnames(),
              testing::Optional(testing::UnorderedElementsAre(
                  HostPortPair("foo.com", 0), HostPortPair("bar.com", 0))));
}

TEST(DnsResponseResultExtractorTest, ExtractsNxdomainPtrResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Hours(5);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypePTR, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)},
      /*additional=*/{}, dns_protocol::kRcodeNXDOMAIN);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsNodataPtrResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Minutes(50);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypePTR, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, RejectsMalformedPtrRecord) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypePTR,
      {BuildTestDnsRecord(kName, dns_protocol::kTypePTR,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kMalformedRecord);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, RejectsWrongNamePtrRecord) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsPointerResponse(
      kName, {"foo.com", "bar.com"}, "different.test");
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypePtrResponses) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypePTR,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, ExtractsSrvResponses) {
  constexpr char kName[] = "name.test";

  const TestServiceRecord kRecord1 = {2, 3, 1223, "foo.com"};
  const TestServiceRecord kRecord2 = {5, 10, 80, "bar.com"};
  const TestServiceRecord kRecord3 = {5, 1, 5, "google.com"};
  const TestServiceRecord kRecord4 = {2, 100, 12345, "chromium.org"};

  DnsResponse response = BuildTestDnsServiceResponse(
      kName, {kRecord1, kRecord2, kRecord3, kRecord4});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());

  // Expect ordered by priority, and random within a priority.
  absl::optional<std::vector<HostPortPair>> result_hosts = results.hostnames();
  ASSERT_THAT(
      result_hosts,
      testing::Optional(testing::UnorderedElementsAre(
          HostPortPair("foo.com", 1223), HostPortPair("bar.com", 80),
          HostPortPair("google.com", 5), HostPortPair("chromium.org", 12345))));
  auto priority2 = std::vector<HostPortPair>(result_hosts.value().begin(),
                                             result_hosts.value().begin() + 2);
  EXPECT_THAT(priority2, testing::UnorderedElementsAre(
                             HostPortPair("foo.com", 1223),
                             HostPortPair("chromium.org", 12345)));
  auto priority5 = std::vector<HostPortPair>(result_hosts.value().begin() + 2,
                                             result_hosts.value().end());
  EXPECT_THAT(priority5,
              testing::UnorderedElementsAre(HostPortPair("bar.com", 80),
                                            HostPortPair("google.com", 5)));
}

// 0-weight services are allowed. Ensure that we can handle such records,
// especially the case where all entries have weight 0.
TEST(DnsResponseResultExtractorTest, ExtractsZeroWeightSrvResponses) {
  constexpr char kName[] = "name.test";

  const TestServiceRecord kRecord1 = {5, 0, 80, "bar.com"};
  const TestServiceRecord kRecord2 = {5, 0, 5, "google.com"};

  DnsResponse response =
      BuildTestDnsServiceResponse(kName, {kRecord1, kRecord2});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());

  // Expect ordered by priority, and random within a priority.
  EXPECT_THAT(results.hostnames(),
              testing::Optional(testing::UnorderedElementsAre(
                  HostPortPair("bar.com", 80), HostPortPair("google.com", 5))));
}

TEST(DnsResponseResultExtractorTest, ExtractsNxdomainSrvResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Days(7);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeSRV, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)},
      /*additional=*/{}, dns_protocol::kRcodeNXDOMAIN);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsNodataSrvResponses) {
  constexpr char kName[] = "name.test";
  constexpr auto kTtl = base::Hours(12);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeSRV, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, RejectsMalformedSrvRecord) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeSRV,
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSRV,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kMalformedRecord);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, RejectsWrongNameSrvRecord) {
  constexpr char kName[] = "name.test";

  const TestServiceRecord kRecord = {2, 3, 1223, "foo.com"};
  DnsResponse response =
      BuildTestDnsServiceResponse(kName, {kRecord}, "different.test");
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypeSrvResponses) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeSRV,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, ExtractsBasicHttpsResponses) {
  constexpr char kName[] = "https.test";
  constexpr auto kTtl = base::Hours(12);

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeHttps,
                           {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                                        /*service_name=*/".",
                                                        /*params=*/{}, kTtl)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre(dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true)));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsComprehensiveHttpsResponses) {
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
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.GetMetadatas(),
              testing::Optional(testing::ElementsAre(
                  ExpectConnectionEndpointMetadata(testing::ElementsAre(kAlpn),
                                                   testing::IsEmpty(), kName),
                  ExpectConnectionEndpointMetadata(
                      testing::ElementsAre(
                          kAlpn, dns_protocol::kHttpsServiceDefaultAlpn),
                      testing::ElementsAreArray(kEchConfig), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true, true)));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, IgnoresHttpsResponseWithAlias) {
  constexpr char kName[] = "https.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeHttps,
                           {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                                        /*service_name=*/".",
                                                        /*params=*/{}),
                            BuildTestHttpsAliasRecord(kName, "alias.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.GetMetadatas(), testing::Optional(testing::IsEmpty()));

  // Expected to still output record compatibility for otherwise-ignored records
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true, true)));
}

// Expect the entire response to be ignored if all HTTPS records have the
// "no-default-alpn" param.
TEST(DnsResponseResultExtractorTest, IgnoresHttpsResponseWithNoDefaultAlpn) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"foo1"}),
            {dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}}),
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/5,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"foo2"}),
            {dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.GetMetadatas(), testing::Optional(testing::IsEmpty()));

  // Expected to still output record compatibility for otherwise-ignored records
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true, true)));
}

// Unsupported/unknown HTTPS params are simply ignored if not marked mandatory.
TEST(DnsResponseResultExtractorTest, IgnoresUnsupportedParamsInHttpsRecord) {
  constexpr char kName[] = "https.test";
  constexpr uint16_t kMadeUpParamKey = 65500;  // From the private-use block.

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {{kMadeUpParamKey, "foo"}})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre(dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true)));
}

// Entire record is dropped if an unsupported/unknown HTTPS param is marked
// mandatory.
TEST(DnsResponseResultExtractorTest,
     IgnoresHttpsRecordWithUnsupportedMandatoryParam) {
  constexpr char kName[] = "https.test";
  constexpr uint16_t kMadeUpParamKey = 65500;  // From the private-use block.

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"ignored_alpn"}),
            BuildTestHttpsServiceMandatoryParam({kMadeUpParamKey}),
            {kMadeUpParamKey, "foo"}}),
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/5,
           /*service_name=*/".",
           /*params=*/{BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(false, true)));
}

TEST(DnsResponseResultExtractorTest,
     ExtractsHttpsRecordWithMatchingServiceName) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/kName,
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST(DnsResponseResultExtractorTest,
     ExtractsHttpsRecordWithMatchingDefaultServiceName) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST(DnsResponseResultExtractorTest,
     ExtractsHttpsRecordWithPrefixedNameAndMatchingServiceName) {
  constexpr char kName[] = "https.test";
  constexpr char kPrefixedName[] = "_444._https.https.test";

  DnsResponse response = BuildTestDnsResponse(
      kPrefixedName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kPrefixedName, /*priority=*/4,
                                   /*service_name=*/kName,
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST(DnsResponseResultExtractorTest,
     ExtractsHttpsRecordWithAliasingAndMatchingServiceName) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestCnameRecord(kName, "alias.test"),
       BuildTestHttpsServiceRecord("alias.test", /*priority=*/4,
                                   /*service_name=*/kName,
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST(DnsResponseResultExtractorTest,
     IgnoreHttpsRecordWithNonMatchingServiceName) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/"other.service.test",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"ignored"})}),
       BuildTestHttpsServiceRecord("https.test", /*priority=*/5,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true, true)));
}

TEST(DnsResponseResultExtractorTest,
     ExtractsHttpsRecordWithPrefixedNameAndDefaultServiceName) {
  constexpr char kPrefixedName[] = "_445._https.https.test";

  DnsResponse response = BuildTestDnsResponse(
      kPrefixedName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kPrefixedName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/"https.test",
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kPrefixedName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST(DnsResponseResultExtractorTest,
     ExtractsHttpsRecordWithAliasingAndDefaultServiceName) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestCnameRecord(kName, "alias.test"),
       BuildTestHttpsServiceRecord("alias.test", /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), "alias.test"))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST(DnsResponseResultExtractorTest, ExtractsHttpsRecordWithMatchingPort) {
  constexpr char kName[] = "https.test";
  constexpr uint16_t kPort = 4567;

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"}),
                                    BuildTestHttpsServicePortParam(kPort)})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/kPort, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST(DnsResponseResultExtractorTest, IgnoresHttpsRecordWithMismatchingPort) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"ignored"}),
                                    BuildTestHttpsServicePortParam(1003)}),
       BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/55, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true, true)));
}

// HTTPS records with "no-default-alpn" but also no "alpn" are not
// "self-consistent" and should be ignored.
TEST(DnsResponseResultExtractorTest, IgnoresHttpsRecordWithNoAlpn) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/".",
           /*params=*/
           {{dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}}),
       BuildTestHttpsServiceRecord(kName, /*priority=*/4,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo"})})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/55, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true, true)));
}

// Expect the entire response to be ignored if all HTTPS records have the
// "no-default-alpn" param.
TEST(DnsResponseResultExtractorTest,
     IgnoresHttpsResponseWithNoCompatibleDefaultAlpn) {
  constexpr char kName[] = "https.test";
  constexpr uint16_t kMadeUpParamKey = 65500;  // From the private-use block.

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsServiceRecord(
           kName, /*priority=*/4,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"foo1"}),
            {dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}}),
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/5,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"foo2"}),
            {dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn, ""}}),
       // Allows default ALPN, but ignored due to non-matching service name.
       BuildTestHttpsServiceRecord(kName, /*priority=*/3,
                                   /*service_name=*/"other.test",
                                   /*params=*/{}),
       // Allows default ALPN, but ignored due to incompatible param.
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/6,
           /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceMandatoryParam({kMadeUpParamKey}),
            {kMadeUpParamKey, "foo"}}),
       // Allows default ALPN, but ignored due to mismatching port.
       BuildTestHttpsServiceRecord(
           kName, /*priority=*/10,
           /*service_name=*/".",
           /*params=*/{BuildTestHttpsServicePortParam(1005)})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.GetMetadatas(), testing::Optional(testing::IsEmpty()));

  // Expected to still output record compatibility for otherwise-ignored records
  EXPECT_THAT(
      results.https_record_compatibility(),
      testing::Pointee(testing::ElementsAre(true, true, true, false, true)));
}

TEST(DnsResponseResultExtractorTest, ExtractsNxdomainHttpsResponses) {
  constexpr char kName[] = "https.test";
  constexpr auto kTtl = base::Minutes(45);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)},
      /*additional=*/{}, dns_protocol::kRcodeNXDOMAIN);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.GetMetadatas(), testing::Optional(testing::IsEmpty()));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::IsEmpty()));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsNodataHttpsResponses) {
  constexpr char kName[] = "https.test";
  constexpr auto kTtl = base::Hours(36);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps, /*answers=*/{},
      /*authority=*/
      {BuildTestDnsRecord(kName, dns_protocol::kTypeSOA, "fake rdata", kTtl)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.GetMetadatas(), testing::Optional(testing::IsEmpty()));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::IsEmpty()));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, RejectsMalformedHttpsRecord) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestDnsRecord(kName, dns_protocol::kTypeHttps,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kMalformedRecord);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, RejectsWrongNameHttpsRecord) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestHttpsAliasRecord("different.test", "alias.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_FALSE(results.has_ttl());
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypeHttpsResponses) {
  constexpr char kName[] = "https.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeHttps,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.GetMetadatas(), testing::Optional(testing::IsEmpty()));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, IgnoresAdditionalHttpsRecords) {
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
      {BuildTestHttpsServiceRecord(kName, /*priority=*/3u, /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo2"})},
                                   base::Minutes(44)),
       BuildTestHttpsServiceRecord(kName, /*priority=*/2u, /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo3"})},
                                   base::Minutes(30))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(
      results.GetMetadatas(),
      testing::Optional(testing::ElementsAre(ExpectConnectionEndpointMetadata(
          testing::ElementsAre("foo1", dns_protocol::kHttpsServiceDefaultAlpn),
          testing::IsEmpty(), kName))));
  EXPECT_THAT(results.https_record_compatibility(),
              testing::Pointee(testing::ElementsAre(true)));

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kTtl);
}

TEST(DnsResponseResultExtractorTest, IgnoresUnsolicitedHttpsRecords) {
  constexpr char kName[] = "name.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestDnsRecord(kName, dns_protocol::kTypeTXT,
                          "\003foo")} /* answers */,
      {} /* authority */,
      {BuildTestHttpsServiceRecord(
           "https.test", /*priority=*/3u, /*service_name=*/".",
           /*params=*/
           {BuildTestHttpsServiceAlpnParam({"foo2"})}, base::Minutes(44)),
       BuildTestHttpsServiceRecord("https.test", /*priority=*/2u,
                                   /*service_name=*/".",
                                   /*params=*/
                                   {BuildTestHttpsServiceAlpnParam({"foo3"})},
                                   base::Minutes(30))} /* additional */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.text_records(),
              testing::Optional(testing::ElementsAre("foo")));
  EXPECT_FALSE(results.GetMetadatas());
  EXPECT_FALSE(results.https_record_compatibility());
}

TEST(DnsResponseResultExtractorTest, HandlesInOrderCnameChain) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord(kName, "second.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestTextRecord("fourth.test", {"bar"})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.text_records(),
              testing::Optional(testing::UnorderedElementsAre("foo", "bar")));
}

TEST(DnsResponseResultExtractorTest, HandlesInOrderCnameChainTypeA) {
  constexpr char kName[] = "first.test";

  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA,
                           {BuildTestCnameRecord(kName, "second.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestAddressRecord("fourth.test", kExpected)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.GetEndpoints(),
              testing::Optional(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(expected_endpoint)))));

  EXPECT_THAT(results.aliases(),
              testing::Pointee(testing::UnorderedElementsAre(
                  "fourth.test", "third.test", "second.test", kName)));
}

TEST(DnsResponseResultExtractorTest, HandlesReverseOrderCnameChain) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.text_records(),
              testing::Optional(testing::ElementsAre("foo")));
}

TEST(DnsResponseResultExtractorTest, HandlesReverseOrderCnameChainTypeA) {
  constexpr char kName[] = "first.test";

  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA,
                           {BuildTestAddressRecord("fourth.test", kExpected),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.GetEndpoints(),
              testing::Optional(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(expected_endpoint)))));

  EXPECT_THAT(results.aliases(),
              testing::Pointee(testing::UnorderedElementsAre(
                  "fourth.test", "third.test", "second.test", kName)));
}

TEST(DnsResponseResultExtractorTest, HandlesArbitraryOrderCnameChain) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.text_records(),
              testing::Optional(testing::ElementsAre("foo")));
}

TEST(DnsResponseResultExtractorTest, HandlesArbitraryOrderCnameChainTypeA) {
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
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.GetEndpoints(),
              testing::Optional(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(expected_endpoint)))));

  EXPECT_THAT(results.aliases(),
              testing::Pointee(testing::UnorderedElementsAre(
                  "zfourth.test", "athird.test", "qsecond.test", kName)));
}

TEST(DnsResponseResultExtractorTest, IgnoresNonResultTypesMixedWithCnameChain) {
  constexpr char kName[] = "first.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestCnameRecord("second.test", "third.test"),
       BuildTestTextRecord("fourth.test", {"foo"}),
       BuildTestCnameRecord("third.test", "fourth.test"),
       BuildTestAddressRecord("third.test", IPAddress(1, 2, 3, 4)),
       BuildTestCnameRecord(kName, "second.test"),
       BuildTestAddressRecord("fourth.test", IPAddress(2, 3, 4, 5))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.text_records(),
              testing::Optional(testing::ElementsAre("foo")));
  EXPECT_FALSE(results.GetEndpoints());
  EXPECT_FALSE(results.aliases());
}

TEST(DnsResponseResultExtractorTest,
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
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_FALSE(results.text_records());
  EXPECT_THAT(results.GetEndpoints(),
              testing::Optional(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(expected_endpoint)))));

  EXPECT_THAT(results.aliases(),
              testing::Pointee(testing::UnorderedElementsAre(
                  "fourth.test", "third.test", "second.test", kName)));
}

TEST(DnsResponseResultExtractorTest, HandlesCnameChainWithoutResult) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.text_records(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, HandlesCnameChainWithoutResultTypeA) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.GetEndpoints(), testing::Optional(testing::IsEmpty()));

  EXPECT_THAT(results.aliases(),
              testing::Pointee(testing::UnorderedElementsAre(
                  "fourth.test", "third.test", "second.test", kName)));
}

TEST(DnsResponseResultExtractorTest, RejectsCnameChainWithLoop) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("third.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "second.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kBadAliasChain);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsCnameChainWithLoopToBeginning) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("third.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "first.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kBadAliasChain);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest,
     RejectsCnameChainWithLoopToBeginningWithoutResult) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "first.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kBadAliasChain);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsCnameChainWithWrongStart) {
  constexpr char kName[] = "test.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kBadAliasChain);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsCnameChainWithWrongResultName) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("third.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsCnameSharedWithResult) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord(kName, {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsDisjointCnameChain) {
  constexpr char kName[] = "first.test";

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestCnameRecord("second.test", "third.test"),
       BuildTestTextRecord("fourth.test", {"foo"}),
       BuildTestCnameRecord("third.test", "fourth.test"),
       BuildTestCnameRecord("other1.test", "other2.test"),
       BuildTestCnameRecord(kName, "second.test"),
       BuildTestCnameRecord("other2.test", "other3.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kBadAliasChain);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsDoubledCnames) {
  constexpr char kName[] = "first.test";

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("third.test", "fifth.test"),
                            BuildTestCnameRecord(kName, "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kMultipleCnames);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, IgnoresTtlFromNonResultType) {
  constexpr char kName[] = "name.test";
  constexpr base::TimeDelta kMinTtl = base::Minutes(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestTextRecord(kName, {"foo"}, base::Hours(3)),
       BuildTestTextRecord(kName, {"bar"}, kMinTtl),
       BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4), base::Seconds(2)),
       BuildTestTextRecord(kName, {"baz"}, base::Minutes(15))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kMinTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsTtlFromCname) {
  constexpr char kName[] = "name.test";
  constexpr char kAlias[] = "alias.test";
  constexpr base::TimeDelta kMinTtl = base::Minutes(4);

  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypeTXT,
      {BuildTestTextRecord(kAlias, {"foo"}, base::Hours(3)),
       BuildTestTextRecord(kAlias, {"bar"}, base::Hours(2)),
       BuildTestTextRecord(kAlias, {"baz"}, base::Minutes(15)),
       BuildTestCnameRecord(kName, kAlias, kMinTtl)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kMinTtl);
}

TEST(DnsResponseResultExtractorTest, ValidatesAliasNames) {
  constexpr char kName[] = "first.test";

  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA,
                           {BuildTestCnameRecord(kName, "second.test"),
                            BuildTestCnameRecord("second.test", "localhost"),
                            BuildTestCnameRecord("localhost", "fourth.test"),
                            BuildTestAddressRecord("fourth.test", kExpected)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.GetEndpoints(),
              testing::Optional(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(expected_endpoint)))));

  // Expect "localhost" to be validated out of the alias list.
  EXPECT_THAT(results.aliases(), testing::Pointee(testing::UnorderedElementsAre(
                                     "fourth.test", "second.test", kName)));
}

TEST(DnsResponseResultExtractorTest, CanonicalizesAliasNames) {
  const IPAddress kExpected(192, 168, 0, 1);
  constexpr char kName[] = "address.test";
  constexpr char kCname[] = "\005ALIAS\004test\000";

  // Need to build records directly in order to manually encode alias target
  // name because BuildTestDnsAddressResponseWithCname() uses DNSDomainFromDot()
  // which does not support non-URL-canonicalized names.
  std::vector<DnsResourceRecord> answers = {
      BuildTestDnsRecord(kName, dns_protocol::kTypeCNAME,
                         std::string(kCname, sizeof(kCname) - 1)),
      BuildTestAddressRecord("alias.test", kExpected)};
  DnsResponse response =
      BuildTestDnsResponse(kName, dns_protocol::kTypeA, answers);

  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A,
                                        /*original_domain_name=*/kName,
                                        /*request_port=*/0, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);
  EXPECT_THAT(results.GetEndpoints(),
              testing::Optional(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(expected_endpoint)))));
  EXPECT_THAT(results.aliases(), testing::Pointee(testing::UnorderedElementsAre(
                                     kName, "alias.test")));
}

}  // namespace
}  // namespace net
