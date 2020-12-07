// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response_result_extractor.h"

#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(DnsResponseResultExtractorTest, ExtractsSingleARecord) {
  constexpr char kName[] = "address.test";
  const IPAddress kExpected(192, 168, 0, 1);

  DnsResponse response = BuildTestDnsAddressResponse(kName, kExpected);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);
  ASSERT_TRUE(results.addresses());
  EXPECT_THAT(results.addresses().value().endpoints(),
              testing::ElementsAre(expected_endpoint));
  EXPECT_EQ(results.addresses().value().GetCanonicalName(), kName);
  EXPECT_THAT(results.addresses().value().dns_aliases(),
              testing::ElementsAre(kName));
}

TEST(DnsResponseResultExtractorTest, ExtractsSingleAAAARecord) {
  constexpr char kName[] = "address.test";

  IPAddress expected;
  CHECK(expected.AssignFromIPLiteral("2001:4860:4860::8888"));

  DnsResponse response = BuildTestDnsAddressResponse(kName, expected);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::AAAA, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  IPEndPoint expected_endpoint(expected, 0 /* port */);
  ASSERT_TRUE(results.addresses());
  EXPECT_THAT(results.addresses().value().endpoints(),
              testing::ElementsAre(expected_endpoint));
  EXPECT_EQ(results.addresses().value().GetCanonicalName(), kName);
  EXPECT_THAT(results.addresses().value().dns_aliases(),
              testing::ElementsAre(kName));
}

TEST(DnsResponseResultExtractorTest, ExtractsSingleARecordWithCname) {
  const IPAddress kExpected(192, 168, 0, 1);
  constexpr char kCanonicalName[] = "alias.test";

  DnsResponse response = BuildTestDnsAddressResponseWithCname(
      "address.test", kExpected, kCanonicalName);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);
  ASSERT_TRUE(results.addresses());
  EXPECT_THAT(results.addresses().value().endpoints(),
              testing::ElementsAre(expected_endpoint));
  EXPECT_EQ(results.addresses().value().GetCanonicalName(), kCanonicalName);
  EXPECT_THAT(results.addresses().value().dns_aliases(),
              testing::ElementsAre(kCanonicalName, "address.test"));
}

TEST(DnsResponseResultExtractorTest, ExtractsARecordsWithCname) {
  DnsResponse response = BuildTestDnsResponse(
      "addresses.test", dns_protocol::kTypeA,
      {
          BuildTestAddressRecord("alias.test", IPAddress(74, 125, 226, 179)),
          BuildTestAddressRecord("alias.test", IPAddress(74, 125, 226, 180)),
          BuildTestCnameRecord("addresses.test", "alias.test"),
          BuildTestAddressRecord("alias.test", IPAddress(74, 125, 226, 176)),
          BuildTestAddressRecord("alias.test", IPAddress(74, 125, 226, 177)),
          BuildTestAddressRecord("alias.test", IPAddress(74, 125, 226, 178)),
      });
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  ASSERT_TRUE(results.addresses());
  EXPECT_THAT(results.addresses().value().endpoints(),
              testing::UnorderedElementsAre(
                  IPEndPoint(IPAddress(74, 125, 226, 179), 0 /* port */),
                  IPEndPoint(IPAddress(74, 125, 226, 178), 0 /* port */),
                  IPEndPoint(IPAddress(74, 125, 226, 180), 0 /* port */),
                  IPEndPoint(IPAddress(74, 125, 226, 176), 0 /* port */),
                  IPEndPoint(IPAddress(74, 125, 226, 177), 0 /* port */)));
  EXPECT_EQ(results.addresses().value().GetCanonicalName(), "alias.test");
  EXPECT_THAT(results.addresses().value().dns_aliases(),
              testing::ElementsAre("alias.test", "addresses.test"));
}

TEST(DnsResponseResultExtractorTest, ExtractsNxdomainAResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "address.test", dns_protocol::kTypeA, {} /* answers */,
      {BuildTestDnsRecord("address.test", dns_protocol::kTypeSOA,
                          "fake rdata")} /* authority */,
      {} /* additional */, dns_protocol::kRcodeNOERROR);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  ASSERT_TRUE(results.addresses());
  EXPECT_TRUE(results.addresses().value().empty());
  EXPECT_EQ(results.addresses().value().GetCanonicalName(), "");
  EXPECT_TRUE(results.addresses().value().dns_aliases().empty());
}

TEST(DnsResponseResultExtractorTest, ExtractsNodataAResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "address.test", dns_protocol::kTypeA, {} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  ASSERT_TRUE(results.addresses());
  EXPECT_TRUE(results.addresses().value().empty());
  EXPECT_EQ(results.addresses().value().GetCanonicalName(), "");
  EXPECT_TRUE(results.addresses().value().dns_aliases().empty());
}

TEST(DnsResponseResultExtractorTest, RejectsMalformedARecord) {
  DnsResponse response = BuildTestDnsResponse(
      "address.test", dns_protocol::kTypeA,
      {BuildTestDnsRecord("address.test", dns_protocol::kTypeA,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kMalformedRecord);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsWrongNameARecord) {
  DnsResponse response = BuildTestDnsAddressResponse(
      "address.test", IPAddress(1, 2, 3, 4), "different.test");
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypeRecordsInAResponse) {
  DnsResponse response = BuildTestDnsResponse(
      "address.test", dns_protocol::kTypeA,
      {BuildTestTextRecord("address.test", {"foo"} /* text_strings */)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  ASSERT_TRUE(results.addresses());
  EXPECT_TRUE(results.addresses().value().empty());
  EXPECT_EQ(results.addresses().value().GetCanonicalName(), "");
  EXPECT_TRUE(results.addresses().value().dns_aliases().empty());
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypeRecordsMixedWithARecords) {
  constexpr char kName[] = "address.test";
  const IPAddress kExpected(8, 8, 8, 8);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA,
      {BuildTestTextRecord(kName, {"foo"} /* text_strings */),
       BuildTestAddressRecord(kName, kExpected)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  ASSERT_TRUE(results.addresses());
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);
  EXPECT_THAT(results.addresses().value().endpoints(),
              testing::ElementsAre(expected_endpoint));
  EXPECT_EQ(results.addresses().value().GetCanonicalName(), kName);
  EXPECT_THAT(results.addresses().value().dns_aliases(),
              testing::ElementsAre(kName));
}

TEST(DnsResponseResultExtractorTest, ExtractsMinATtl) {
  constexpr char kName[] = "name.test";
  constexpr base::TimeDelta kMinTtl = base::TimeDelta::FromMinutes(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeA,
      {BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4),
                              base::TimeDelta::FromHours(3)),
       BuildTestAddressRecord(kName, IPAddress(2, 3, 4, 5), kMinTtl),
       BuildTestAddressRecord(kName, IPAddress(3, 4, 5, 6),
                              base::TimeDelta::FromMinutes(15))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kMinTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsTxtResponses) {
  // Simulate two separate DNS records, each with multiple strings.
  std::vector<std::string> foo_records = {"foo1", "foo2", "foo3"};
  std::vector<std::string> bar_records = {"bar1", "bar2"};
  std::vector<std::vector<std::string>> text_records = {foo_records,
                                                        bar_records};

  DnsResponse response =
      BuildTestDnsTextResponse("name.test", std::move(text_records));
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());

  // Order between separate DNS records is undefined, but each record should
  // stay in order as that order may be meaningful.
  ASSERT_THAT(results.text_records(),
              testing::Optional(testing::UnorderedElementsAre(
                  "foo1", "foo2", "foo3", "bar1", "bar2")));
  std::vector<std::string> results_vector = results.text_records().value();
  EXPECT_NE(results_vector.end(),
            std::search(results_vector.begin(), results_vector.end(),
                        foo_records.begin(), foo_records.end()));
  EXPECT_NE(results_vector.end(),
            std::search(results_vector.begin(), results_vector.end(),
                        bar_records.begin(), bar_records.end()));
}

TEST(DnsResponseResultExtractorTest, ExtractsNxdomainTxtResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypeTXT, {} /* answers */,
      {BuildTestDnsRecord("name.test", dns_protocol::kTypeSOA,
                          "fake rdata")} /* authority */,
      {} /* additional */, dns_protocol::kRcodeNOERROR);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.text_records(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, ExtractsNodataTxtResponses) {
  DnsResponse response =
      BuildTestDnsTextResponse("name.test", {} /* text_records */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.text_records(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, RejectsMalformedTxtRecord) {
  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypeTXT,
      {BuildTestDnsRecord("name.test", dns_protocol::kTypeTXT,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kMalformedRecord);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsWrongNameTxtRecord) {
  DnsResponse response =
      BuildTestDnsTextResponse("name.test", {{"foo"}}, "different.test");
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypeTxtResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypeTXT,
      {BuildTestAddressRecord("name.test", IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.text_records(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, ExtractsMinTxtTtl) {
  constexpr char kName[] = "name.test";
  constexpr base::TimeDelta kMinTtl = base::TimeDelta::FromMinutes(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestTextRecord(kName, {"foo"}, base::TimeDelta::FromHours(3)),
       BuildTestTextRecord(kName, {"bar"}, kMinTtl),
       BuildTestTextRecord(kName, {"baz"}, base::TimeDelta::FromMinutes(15))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kMinTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsPtrResponses) {
  DnsResponse response =
      BuildTestDnsPointerResponse("name.test", {"foo.com", "bar.com"});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());

  // Order between separate records is undefined.
  EXPECT_THAT(results.hostnames(),
              testing::Optional(testing::UnorderedElementsAre(
                  HostPortPair("foo.com", 0), HostPortPair("bar.com", 0))));
}

TEST(DnsResponseResultExtractorTest, ExtractsNxdomainPtrResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypePTR, {} /* answers */,
      {BuildTestDnsRecord("name.test", dns_protocol::kTypeSOA,
                          "fake rdata")} /* authority */,
      {} /* additional */, dns_protocol::kRcodeNOERROR);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, ExtractsNodataPtrResponses) {
  DnsResponse response =
      BuildTestDnsPointerResponse("name.test", {} /* pointer_names */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, RejectsMalformedPtrRecord) {
  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypePTR,
      {BuildTestDnsRecord("name.test", dns_protocol::kTypePTR,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR, &results),
            DnsResponseResultExtractor::ExtractionError::kMalformedRecord);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsWrongNamePtrRecord) {
  DnsResponse response = BuildTestDnsPointerResponse(
      "name.test", {"foo.com", "bar.com"}, "different.test");
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypePtrResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypePTR,
      {BuildTestAddressRecord("name.test", IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::PTR, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, ExtractsSrvResponses) {
  const TestServiceRecord kRecord1 = {2, 3, 1223, "foo.com"};
  const TestServiceRecord kRecord2 = {5, 10, 80, "bar.com"};
  const TestServiceRecord kRecord3 = {5, 1, 5, "google.com"};
  const TestServiceRecord kRecord4 = {2, 100, 12345, "chromium.org"};

  DnsResponse response = BuildTestDnsServiceResponse(
      "name.test", {kRecord1, kRecord2, kRecord3, kRecord4});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());

  // Expect ordered by priority, and random within a priority.
  base::Optional<std::vector<HostPortPair>> result_hosts = results.hostnames();
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
  const TestServiceRecord kRecord1 = {5, 0, 80, "bar.com"};
  const TestServiceRecord kRecord2 = {5, 0, 5, "google.com"};

  DnsResponse response =
      BuildTestDnsServiceResponse("name.test", {kRecord1, kRecord2});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());

  // Expect ordered by priority, and random within a priority.
  EXPECT_THAT(results.hostnames(),
              testing::Optional(testing::UnorderedElementsAre(
                  HostPortPair("bar.com", 80), HostPortPair("google.com", 5))));
}

TEST(DnsResponseResultExtractorTest, ExtractsNxdomainSrvResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypeSRV, {} /* answers */,
      {BuildTestDnsRecord("name.test", dns_protocol::kTypeSOA,
                          "fake rdata")} /* authority */,
      {} /* additional */, dns_protocol::kRcodeNOERROR);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, ExtractsNodataSrvResponses) {
  DnsResponse response =
      BuildTestDnsServiceResponse("name.test", {} /* service_records */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, RejectsMalformedSrvRecord) {
  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypeSRV,
      {BuildTestDnsRecord("name.test", dns_protocol::kTypeSRV,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV, &results),
            DnsResponseResultExtractor::ExtractionError::kMalformedRecord);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsWrongNameSrvRecord) {
  const TestServiceRecord kRecord = {2, 3, 1223, "foo.com"};
  DnsResponse response =
      BuildTestDnsServiceResponse("name.test", {kRecord}, "different.test");
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypeSrvResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypeSRV,
      {BuildTestAddressRecord("name.test", IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::SRV, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.hostnames(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, ExtractsHttpsResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "https.test", dns_protocol::kTypeHttps,
      {BuildTestHttpsAliasRecord("https.test", "alias.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  // Experimental type, so does not affect overall result.
  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_THAT(results.experimental_results(),
              testing::Optional(testing::ElementsAre(true)));
}

TEST(DnsResponseResultExtractorTest, ExtractsNxdomainHttpsResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "https.test", dns_protocol::kTypeHttps, {} /* answers */,
      {BuildTestDnsRecord("name.test", dns_protocol::kTypeSOA,
                          "fake rdata")} /* authority */,
      {} /* additional */, dns_protocol::kRcodeNOERROR);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.experimental_results(),
              testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, ExtractsNodataHttpsResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "https.test", dns_protocol::kTypeHttps, {} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.experimental_results(),
              testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, RecognizesMalformedHttpsRecord) {
  DnsResponse response = BuildTestDnsResponse(
      "https.test", dns_protocol::kTypeHttps,
      {BuildTestDnsRecord("https.test", dns_protocol::kTypeHttps,
                          "malformed rdata")} /* answers */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.experimental_results(),
              testing::Optional(testing::ElementsAre(false)));
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongNameHttpsRecord) {
  DnsResponse response = BuildTestDnsResponse(
      "https.test", dns_protocol::kTypeHttps,
      {BuildTestHttpsAliasRecord("different.test", "alias.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));

  HostCache::Entry default_results =
      DnsResponseResultExtractor::CreateEmptyResult(DnsQueryType::HTTPS);
  EXPECT_EQ(results.error(), default_results.error());
  EXPECT_EQ(results.experimental_results(),
            default_results.experimental_results());
}

TEST(DnsResponseResultExtractorTest, IgnoresWrongTypeHttpsResponses) {
  DnsResponse response = BuildTestDnsResponse(
      "https.test", dns_protocol::kTypeHttps,
      {BuildTestAddressRecord("https.test", IPAddress(1, 2, 3, 4))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.experimental_results(),
              testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, IgnoresAdditionalHttpsRecords) {
  DnsResponse response = BuildTestDnsResponse(
      "https.test", dns_protocol::kTypeHttps,
      {BuildTestHttpsAliasRecord("https.test", "alias.test")} /* answers */,
      {} /* authority */,
      {BuildTestHttpsServiceRecord("https.test", 3u, "service1.test", {}),
       BuildTestHttpsServiceRecord("https.test", 2u, "service2.test",
                                   {})} /* additional */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::HTTPS, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  // Experimental type, so does not affect overall result.
  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_THAT(results.experimental_results(),
              testing::Optional(testing::ElementsAre(true)));
}

TEST(DnsResponseResultExtractorTest, IgnoresUnsolicitedHttpsRecords) {
  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypeTXT,
      {BuildTestDnsRecord("name.test", dns_protocol::kTypeTXT,
                          "\003foo")} /* answers */,
      {} /* authority */,
      {BuildTestHttpsServiceRecord("https.test", 3u, "service1.test", {}),
       BuildTestHttpsServiceRecord("https.test", 2u, "service2.test",
                                   {})} /* additional */);
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.text_records(),
              testing::Optional(testing::ElementsAre("foo")));
}

TEST(DnsResponseResultExtractorTest, HandlesInOrderCnameChain) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("first.test", "second.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestTextRecord("fourth.test", {"bar"})});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.text_records(),
              testing::Optional(testing::UnorderedElementsAre("foo", "bar")));
}

TEST(DnsResponseResultExtractorTest, HandlesInOrderCnameChainTypeA) {
  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeA,
                           {BuildTestCnameRecord("first.test", "second.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestAddressRecord("fourth.test", kExpected)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  ASSERT_TRUE(results.addresses());
  EXPECT_THAT(results.addresses().value().endpoints(),
              testing::ElementsAre(expected_endpoint));

  EXPECT_THAT(results.addresses().value().dns_aliases(),
              testing::ElementsAre("fourth.test", "third.test", "second.test",
                                   "first.test"));
}

TEST(DnsResponseResultExtractorTest, HandlesReverseOrderCnameChain) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeTXT,
                           {BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.text_records(),
              testing::Optional(testing::ElementsAre("foo")));
}

TEST(DnsResponseResultExtractorTest, HandlesReverseOrderCnameChainTypeA) {
  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeA,
                           {BuildTestAddressRecord("fourth.test", kExpected),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  ASSERT_TRUE(results.addresses());
  EXPECT_THAT(results.addresses().value().endpoints(),
              testing::ElementsAre(expected_endpoint));

  EXPECT_THAT(results.addresses().value().dns_aliases(),
              testing::ElementsAre("fourth.test", "third.test", "second.test",
                                   "first.test"));
}

TEST(DnsResponseResultExtractorTest, HandlesArbitraryOrderCnameChain) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.text_records(),
              testing::Optional(testing::ElementsAre("foo")));
}

TEST(DnsResponseResultExtractorTest, HandlesArbitraryOrderCnameChainTypeA) {
  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  // Alias names are chosen so that the chain order is not in alphabetical
  // order.
  DnsResponse response = BuildTestDnsResponse(
      "first.test", dns_protocol::kTypeA,
      {BuildTestCnameRecord("qsecond.test", "athird.test"),
       BuildTestAddressRecord("zfourth.test", kExpected),
       BuildTestCnameRecord("athird.test", "zfourth.test"),
       BuildTestCnameRecord("first.test", "qsecond.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  ASSERT_TRUE(results.addresses());
  EXPECT_THAT(results.addresses().value().endpoints(),
              testing::ElementsAre(expected_endpoint));

  EXPECT_THAT(results.addresses().value().dns_aliases(),
              testing::ElementsAre("zfourth.test", "athird.test",
                                   "qsecond.test", "first.test"));
}

TEST(DnsResponseResultExtractorTest, IgnoresNonResultTypesMixedWithCnameChain) {
  DnsResponse response = BuildTestDnsResponse(
      "first.test", dns_protocol::kTypeTXT,
      {BuildTestCnameRecord("second.test", "third.test"),
       BuildTestTextRecord("fourth.test", {"foo"}),
       BuildTestCnameRecord("third.test", "fourth.test"),
       BuildTestAddressRecord("third.test", IPAddress(1, 2, 3, 4)),
       BuildTestCnameRecord("first.test", "second.test"),
       BuildTestAddressRecord("fourth.test", IPAddress(2, 3, 4, 5))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_THAT(results.text_records(),
              testing::Optional(testing::ElementsAre("foo")));
  EXPECT_FALSE(results.addresses());
}

TEST(DnsResponseResultExtractorTest,
     IgnoresNonResultTypesMixedWithCnameChainTypeA) {
  const IPAddress kExpected(192, 168, 0, 1);
  IPEndPoint expected_endpoint(kExpected, 0 /* port */);

  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeA,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("first.test", "second.test"),
                            BuildTestAddressRecord("fourth.test", kExpected)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsOk());
  EXPECT_FALSE(results.text_records());
  ASSERT_TRUE(results.addresses());
  EXPECT_THAT(results.addresses().value().endpoints(),
              testing::ElementsAre(expected_endpoint));

  EXPECT_THAT(results.addresses().value().dns_aliases(),
              testing::ElementsAre("fourth.test", "third.test", "second.test",
                                   "first.test"));
}

TEST(DnsResponseResultExtractorTest, HandlesCnameChainWithoutResult) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(results.text_records(), testing::Optional(testing::IsEmpty()));
}

TEST(DnsResponseResultExtractorTest, HandlesCnameChainWithoutResultTypeA) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeA,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::A, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  EXPECT_THAT(results.error(), test::IsError(ERR_NAME_NOT_RESOLVED));
  ASSERT_TRUE(results.addresses());
  EXPECT_TRUE(results.addresses().value().dns_aliases().empty());
}

TEST(DnsResponseResultExtractorTest, RejectsCnameChainWithLoop) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("third.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "second.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kBadAliasChain);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsCnameChainWithLoopToBeginning) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("third.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "first.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kBadAliasChain);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest,
     RejectsCnameChainWithLoopToBeginningWithoutResult) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestCnameRecord("third.test", "first.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kBadAliasChain);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsCnameChainWithWrongStart) {
  DnsResponse response =
      BuildTestDnsResponse("test.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kBadAliasChain);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsCnameChainWithWrongResultName) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("third.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsCnameSharedWithResult) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("first.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kNameMismatch);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsDisjointCnameChain) {
  DnsResponse response = BuildTestDnsResponse(
      "first.test", dns_protocol::kTypeTXT,
      {BuildTestCnameRecord("second.test", "third.test"),
       BuildTestTextRecord("fourth.test", {"foo"}),
       BuildTestCnameRecord("third.test", "fourth.test"),
       BuildTestCnameRecord("other1.test", "other2.test"),
       BuildTestCnameRecord("first.test", "second.test"),
       BuildTestCnameRecord("other2.test", "other3.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kBadAliasChain);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, RejectsDoubledCnames) {
  DnsResponse response =
      BuildTestDnsResponse("first.test", dns_protocol::kTypeTXT,
                           {BuildTestCnameRecord("second.test", "third.test"),
                            BuildTestTextRecord("fourth.test", {"foo"}),
                            BuildTestCnameRecord("third.test", "fourth.test"),
                            BuildTestCnameRecord("third.test", "fifth.test"),
                            BuildTestCnameRecord("first.test", "second.test")});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kMultipleCnames);

  EXPECT_THAT(results.error(), test::IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST(DnsResponseResultExtractorTest, IgnoresTtlFromNonResultType) {
  constexpr char kName[] = "name.test";
  constexpr base::TimeDelta kMinTtl = base::TimeDelta::FromMinutes(4);

  DnsResponse response = BuildTestDnsResponse(
      kName, dns_protocol::kTypeTXT,
      {BuildTestTextRecord(kName, {"foo"}, base::TimeDelta::FromHours(3)),
       BuildTestTextRecord(kName, {"bar"}, kMinTtl),
       BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4),
                              base::TimeDelta::FromSeconds(2)),
       BuildTestTextRecord(kName, {"baz"}, base::TimeDelta::FromMinutes(15))});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kMinTtl);
}

TEST(DnsResponseResultExtractorTest, ExtractsTtlFromCname) {
  constexpr char kAlias[] = "alias.test";
  constexpr base::TimeDelta kMinTtl = base::TimeDelta::FromMinutes(4);

  DnsResponse response = BuildTestDnsResponse(
      "name.test", dns_protocol::kTypeTXT,
      {BuildTestTextRecord(kAlias, {"foo"}, base::TimeDelta::FromHours(3)),
       BuildTestTextRecord(kAlias, {"bar"}, base::TimeDelta::FromHours(2)),
       BuildTestTextRecord(kAlias, {"baz"}, base::TimeDelta::FromMinutes(15)),
       BuildTestCnameRecord("name.test", kAlias, kMinTtl)});
  DnsResponseResultExtractor extractor(&response);

  HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  EXPECT_EQ(extractor.ExtractDnsResults(DnsQueryType::TXT, &results),
            DnsResponseResultExtractor::ExtractionError::kOk);

  ASSERT_TRUE(results.has_ttl());
  EXPECT_EQ(results.ttl(), kMinTtl);
}

}  // namespace
}  // namespace net
