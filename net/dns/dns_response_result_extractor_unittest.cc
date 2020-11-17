// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response_result_extractor.h"

#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "net/base/host_port_pair.h"
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

}  // namespace
}  // namespace net
