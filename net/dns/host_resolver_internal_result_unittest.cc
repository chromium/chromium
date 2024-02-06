// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_internal_result.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_query_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Optional;
using ::testing::Ref;

namespace net {
namespace {

TEST(HostResolverInternalResultTest, DeserializeMalformedValue) {
  base::Value non_dict(base::Value::Type::BOOLEAN);
  EXPECT_FALSE(HostResolverInternalResult::FromValue(non_dict));

  base::Value missing_type(base::Value::Type::DICT);
  EXPECT_FALSE(HostResolverInternalResult::FromValue(missing_type));

  base::Value bad_type(base::Value::Type::DICT);
  bad_type.GetDict().Set("type", "foo");
  EXPECT_FALSE(HostResolverInternalResult::FromValue(bad_type));
}

TEST(HostResolverInternalResultTest, DataResult) {
  auto result = std::make_unique<HostResolverInternalDataResult>(
      "domain.test", DnsQueryType::AAAA, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{IPEndPoint(IPAddress(2, 2, 2, 2), 46)},
      std::vector<std::string>{"foo", "bar"},
      std::vector<HostPortPair>{HostPortPair("anotherdomain.test", 112)});

  EXPECT_EQ(result->domain_name(), "domain.test");
  EXPECT_EQ(result->query_type(), DnsQueryType::AAAA);
  EXPECT_EQ(result->type(), HostResolverInternalResult::Type::kData);
  EXPECT_EQ(result->source(), HostResolverInternalResult::Source::kDns);
  EXPECT_THAT(result->expiration(), Optional(base::TimeTicks()));
  EXPECT_THAT(result->timed_expiration(), Optional(base::Time()));

  EXPECT_THAT(result->AsData(), Ref(*result));

  EXPECT_THAT(result->endpoints(),
              ElementsAre(IPEndPoint(IPAddress(2, 2, 2, 2), 46)));
  EXPECT_THAT(result->strings(), ElementsAre("foo", "bar"));
  EXPECT_THAT(result->hosts(),
              ElementsAre(HostPortPair("anotherdomain.test", 112)));
}

TEST(HostResolverInternalResultTest, CloneDataResult) {
  auto result = std::make_unique<HostResolverInternalDataResult>(
      "domain.test", DnsQueryType::AAAA, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{IPEndPoint(IPAddress(2, 2, 2, 2), 46)},
      std::vector<std::string>{"foo", "bar"},
      std::vector<HostPortPair>{HostPortPair("anotherdomain.test", 112)});

  std::unique_ptr<HostResolverInternalResult> copy = result->Clone();
  EXPECT_NE(copy.get(), result.get());

  EXPECT_EQ(copy->domain_name(), "domain.test");
  EXPECT_EQ(copy->query_type(), DnsQueryType::AAAA);
  EXPECT_EQ(copy->type(), HostResolverInternalResult::Type::kData);
  EXPECT_EQ(copy->source(), HostResolverInternalResult::Source::kDns);
  EXPECT_THAT(copy->expiration(), Optional(base::TimeTicks()));
  EXPECT_THAT(copy->timed_expiration(), Optional(base::Time()));
  EXPECT_THAT(copy->AsData().endpoints(),
              ElementsAre(IPEndPoint(IPAddress(2, 2, 2, 2), 46)));
  EXPECT_THAT(copy->AsData().strings(), ElementsAre("foo", "bar"));
  EXPECT_THAT(copy->AsData().hosts(),
              ElementsAre(HostPortPair("anotherdomain.test", 112)));
}

TEST(HostResolverInternalResultTest, RoundtripDataResultThroughSerialization) {
  auto result = std::make_unique<HostResolverInternalDataResult>(
      "domain.test", DnsQueryType::AAAA, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{IPEndPoint(IPAddress(2, 2, 2, 2), 46)},
      std::vector<std::string>{"foo", "bar"},
      std::vector<HostPortPair>{HostPortPair("anotherdomain.test", 112)});

  base::Value value = result->ToValue();
  auto deserialized = HostResolverInternalResult::FromValue(value);
  ASSERT_TRUE(deserialized);
  ASSERT_EQ(deserialized->type(), HostResolverInternalResult::Type::kData);

  // Expect deserialized result to be the same as the original other than
  // missing non-timed expiration.
  EXPECT_EQ(deserialized->AsData(),
            HostResolverInternalDataResult(
                result->domain_name(), result->query_type(),
                /*expiration=*/std::nullopt, result->timed_expiration().value(),
                result->source(), result->endpoints(), result->strings(),
                result->hosts()));
}

// Expect results to serialize to a consistent base::Value format for
// consumption by NetLog and similar.
TEST(HostResolverInternalResultTest, SerializepDataResult) {
  auto result = std::make_unique<HostResolverInternalDataResult>(
      "domain.test", DnsQueryType::AAAA, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{IPEndPoint(IPAddress(2, 2, 2, 2), 46)},
      std::vector<std::string>{"foo", "bar"},
      std::vector<HostPortPair>{HostPortPair("anotherdomain.test", 112)});
  base::Value value = result->ToValue();

  std::optional<base::Value> expected = base::JSONReader::Read(
      R"(
        {
          "domain_name": "domain.test",
          "endpoints": [
            {
              "address": "2.2.2.2",
              "port": 46
            }
          ],
          "hosts": [
            {
              "host": "anotherdomain.test",
              "port": 112
            }
          ],
          "query_type": "AAAA",
          "source": "dns",
          "strings": [
            "foo",
            "bar"
          ],
          "timed_expiration": "0",
          "type": "data"
        }
        )");
  ASSERT_TRUE(expected.has_value());

  EXPECT_EQ(value, expected.value());
}

TEST(HostResolverInternalResultTest, DeserializeMalformedDataValue) {
  auto result = std::make_unique<HostResolverInternalDataResult>(
      "domain.test", DnsQueryType::AAAA, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{IPEndPoint(IPAddress(2, 2, 2, 2), 46)},
      std::vector<std::string>{"foo", "bar"},
      std::vector<HostPortPair>{HostPortPair("anotherdomain.test", 112)});
  base::Value valid_value = result->ToValue();
  ASSERT_TRUE(HostResolverInternalDataResult::FromValue(valid_value));

  base::Value missing_domain = valid_value.Clone();
  ASSERT_TRUE(missing_domain.GetDict().Remove("domain_name"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(missing_domain));

  base::Value missing_qtype = valid_value.Clone();
  ASSERT_TRUE(missing_qtype.GetDict().Remove("query_type"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(missing_qtype));
  base::Value unknown_qtype = valid_value.Clone();
  ASSERT_TRUE(unknown_qtype.GetDict().Set("query_type", "foo"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(unknown_qtype));

  base::Value missing_value_type = valid_value.Clone();
  ASSERT_TRUE(missing_value_type.GetDict().Remove("type"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(missing_value_type));
  base::Value unknown_value_type = valid_value.Clone();
  ASSERT_TRUE(unknown_value_type.GetDict().Set("type", "foo"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(unknown_value_type));

  base::Value missing_source = valid_value.Clone();
  ASSERT_TRUE(missing_source.GetDict().Remove("source"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(missing_source));
  base::Value unknown_source = valid_value.Clone();
  ASSERT_TRUE(unknown_source.GetDict().Set("source", "foo"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(unknown_source));

  base::Value missing_expiration = valid_value.Clone();
  ASSERT_TRUE(missing_expiration.GetDict().Remove("timed_expiration"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(missing_expiration));
  base::Value invalid_expiration = valid_value.Clone();
  ASSERT_TRUE(invalid_expiration.GetDict().Set("timed_expiration", "foo"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(invalid_expiration));

  base::Value missing_endpoints = valid_value.Clone();
  ASSERT_TRUE(missing_endpoints.GetDict().Remove("endpoints"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(missing_endpoints));
  base::Value invalid_endpoint = valid_value.Clone();
  invalid_endpoint.GetDict().FindList("endpoints")->front() =
      base::Value("foo");
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(invalid_endpoint));

  base::Value missing_strings = valid_value.Clone();
  ASSERT_TRUE(missing_strings.GetDict().Remove("strings"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(missing_strings));
  base::Value invalid_string = valid_value.Clone();
  invalid_string.GetDict().FindList("strings")->front() = base::Value(5);
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(invalid_string));

  base::Value missing_hosts = valid_value.Clone();
  ASSERT_TRUE(missing_hosts.GetDict().Remove("hosts"));
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(missing_hosts));
  base::Value invalid_hosts = valid_value.Clone();
  invalid_hosts.GetDict().FindList("hosts")->front() = base::Value("foo");
  EXPECT_FALSE(HostResolverInternalDataResult::FromValue(invalid_hosts));
}

TEST(HostResolverInternalResultTest, MetadataResult) {
  const ConnectionEndpointMetadata kMetadata(
      /*supported_protocol_alpns=*/{"http/1.1", "h3"},
      /*ech_config_list=*/{0x01, 0x13},
      /*target_name*/ "target.test");
  auto result = std::make_unique<HostResolverInternalMetadataResult>(
      "domain1.test", DnsQueryType::HTTPS, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{
          {4, kMetadata}});

  EXPECT_EQ(result->domain_name(), "domain1.test");
  EXPECT_EQ(result->query_type(), DnsQueryType::HTTPS);
  EXPECT_EQ(result->type(), HostResolverInternalResult::Type::kMetadata);
  EXPECT_EQ(result->source(), HostResolverInternalResult::Source::kDns);
  EXPECT_THAT(result->expiration(), Optional(base::TimeTicks()));
  EXPECT_THAT(result->timed_expiration(), Optional(base::Time()));

  EXPECT_THAT(result->AsMetadata(), Ref(*result));

  EXPECT_THAT(result->metadatas(), ElementsAre(std::pair(4, kMetadata)));
}

TEST(HostResolverInternalResultTest, CloneMetadataResult) {
  const ConnectionEndpointMetadata kMetadata(
      /*supported_protocol_alpns=*/{"http/1.1", "h3"},
      /*ech_config_list=*/{0x01, 0x13},
      /*target_name*/ "target.test");
  auto result = std::make_unique<HostResolverInternalMetadataResult>(
      "domain1.test", DnsQueryType::HTTPS, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{
          {4, kMetadata}});

  std::unique_ptr<HostResolverInternalResult> copy = result->Clone();
  EXPECT_NE(copy.get(), result.get());

  EXPECT_EQ(copy->domain_name(), "domain1.test");
  EXPECT_EQ(copy->query_type(), DnsQueryType::HTTPS);
  EXPECT_EQ(copy->type(), HostResolverInternalResult::Type::kMetadata);
  EXPECT_EQ(copy->source(), HostResolverInternalResult::Source::kDns);
  EXPECT_THAT(copy->expiration(), Optional(base::TimeTicks()));
  EXPECT_THAT(copy->timed_expiration(), Optional(base::Time()));
  EXPECT_THAT(copy->AsMetadata().metadatas(),
              ElementsAre(std::make_pair(4, kMetadata)));
}

TEST(HostResolverInternalResultTest,
     RoundtripMetadataResultThroughSerialization) {
  const ConnectionEndpointMetadata kMetadata(
      /*supported_protocol_alpns=*/{"http/1.1", "h2", "h3"},
      /*ech_config_list=*/{0x01, 0x13, 0x15},
      /*target_name*/ "target1.test");
  auto result = std::make_unique<HostResolverInternalMetadataResult>(
      "domain2.test", DnsQueryType::HTTPS, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{
          {2, kMetadata}});

  base::Value value = result->ToValue();
  auto deserialized = HostResolverInternalResult::FromValue(value);
  ASSERT_TRUE(deserialized);
  ASSERT_EQ(deserialized->type(), HostResolverInternalResult::Type::kMetadata);

  // Expect deserialized result to be the same as the original other than
  // missing non-timed expiration.
  EXPECT_EQ(deserialized->AsMetadata(),
            HostResolverInternalMetadataResult(
                result->domain_name(), result->query_type(),
                /*expiration=*/std::nullopt, result->timed_expiration().value(),
                result->source(), result->metadatas()));
}

// Expect results to serialize to a consistent base::Value format for
// consumption by NetLog and similar.
TEST(HostResolverInternalResultTest, SerializepMetadataResult) {
  const ConnectionEndpointMetadata kMetadata(
      /*supported_protocol_alpns=*/{"http/1.1", "h2", "h3"},
      /*ech_config_list=*/{0x01, 0x13, 0x15},
      /*target_name*/ "target1.test");
  auto result = std::make_unique<HostResolverInternalMetadataResult>(
      "domain2.test", DnsQueryType::HTTPS, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{
          {2, kMetadata}});
  base::Value value = result->ToValue();

  // Note that the `ech_config_list` base64 encodes to "ARMV".
  std::optional<base::Value> expected = base::JSONReader::Read(
      R"(
        {
          "domain_name": "domain2.test",
          "metadatas": [
            {
              "metadata_value":
              {
                "ech_config_list": "ARMV",
                "supported_protocol_alpns": ["http/1.1", "h2", "h3"],
                "target_name": "target1.test"
              },
              "metadata_weight": 2
            }
          ],
          "query_type": "HTTPS",
          "source": "dns",
          "timed_expiration": "0",
          "type": "metadata"
        }
        )");
  ASSERT_TRUE(expected.has_value());

  EXPECT_EQ(value, expected.value());
}

TEST(HostResolverInternalResultTest, DeserializeMalformedMetadataValue) {
  const ConnectionEndpointMetadata kMetadata(
      /*supported_protocol_alpns=*/{"http/1.1", "h2", "h3"},
      /*ech_config_list=*/{0x01, 0x13, 0x15},
      /*target_name*/ "target1.test");
  auto result = std::make_unique<HostResolverInternalMetadataResult>(
      "domain2.test", DnsQueryType::HTTPS, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{
          {2, kMetadata}});
  base::Value valid_value = result->ToValue();
  ASSERT_TRUE(HostResolverInternalMetadataResult::FromValue(valid_value));

  base::Value missing_domain = valid_value.Clone();
  ASSERT_TRUE(missing_domain.GetDict().Remove("domain_name"));
  EXPECT_FALSE(HostResolverInternalMetadataResult::FromValue(missing_domain));

  base::Value missing_qtype = valid_value.Clone();
  ASSERT_TRUE(missing_qtype.GetDict().Remove("query_type"));
  EXPECT_FALSE(HostResolverInternalMetadataResult::FromValue(missing_qtype));
  base::Value unknown_qtype = valid_value.Clone();
  ASSERT_TRUE(unknown_qtype.GetDict().Set("query_type", "foo"));
  EXPECT_FALSE(HostResolverInternalMetadataResult::FromValue(unknown_qtype));

  base::Value missing_value_type = valid_value.Clone();
  ASSERT_TRUE(missing_value_type.GetDict().Remove("type"));
  EXPECT_FALSE(
      HostResolverInternalMetadataResult::FromValue(missing_value_type));
  base::Value unknown_value_type = valid_value.Clone();
  ASSERT_TRUE(unknown_value_type.GetDict().Set("type", "foo"));
  EXPECT_FALSE(
      HostResolverInternalMetadataResult::FromValue(unknown_value_type));

  base::Value missing_source = valid_value.Clone();
  ASSERT_TRUE(missing_source.GetDict().Remove("source"));
  EXPECT_FALSE(HostResolverInternalMetadataResult::FromValue(missing_source));
  base::Value unknown_source = valid_value.Clone();
  ASSERT_TRUE(unknown_source.GetDict().Set("source", "foo"));
  EXPECT_FALSE(HostResolverInternalMetadataResult::FromValue(unknown_source));

  base::Value missing_expiration = valid_value.Clone();
  ASSERT_TRUE(missing_expiration.GetDict().Remove("timed_expiration"));
  EXPECT_FALSE(
      HostResolverInternalMetadataResult::FromValue(missing_expiration));
  base::Value invalid_expiration = valid_value.Clone();
  ASSERT_TRUE(invalid_expiration.GetDict().Set("timed_expiration", "foo"));
  EXPECT_FALSE(
      HostResolverInternalMetadataResult::FromValue(invalid_expiration));

  base::Value missing_metadatas = valid_value.Clone();
  ASSERT_TRUE(missing_metadatas.GetDict().Remove("metadatas"));
  EXPECT_FALSE(
      HostResolverInternalMetadataResult::FromValue(missing_metadatas));
  base::Value invalid_metadatas = valid_value.Clone();
  *invalid_metadatas.GetDict().Find("metadatas") = base::Value(4);
  EXPECT_FALSE(
      HostResolverInternalMetadataResult::FromValue(invalid_metadatas));

  base::Value missing_weight = valid_value.Clone();
  ASSERT_TRUE(missing_weight.GetDict()
                  .Find("metadatas")
                  ->GetList()
                  .front()
                  .GetDict()
                  .Remove("metadata_weight"));
  EXPECT_FALSE(HostResolverInternalMetadataResult::FromValue(missing_weight));
  base::Value invalid_weight = valid_value.Clone();
  *invalid_weight.GetDict()
       .Find("metadatas")
       ->GetList()
       .front()
       .GetDict()
       .Find("metadata_weight") = base::Value("foo");
  EXPECT_FALSE(HostResolverInternalMetadataResult::FromValue(invalid_weight));

  base::Value missing_value = valid_value.Clone();
  ASSERT_TRUE(missing_value.GetDict()
                  .Find("metadatas")
                  ->GetList()
                  .front()
                  .GetDict()
                  .Remove("metadata_value"));
  EXPECT_FALSE(HostResolverInternalMetadataResult::FromValue(missing_value));
  base::Value invalid_value = valid_value.Clone();
  *invalid_value.GetDict()
       .Find("metadatas")
       ->GetList()
       .front()
       .GetDict()
       .Find("metadata_value") = base::Value("foo");
  EXPECT_FALSE(HostResolverInternalMetadataResult::FromValue(invalid_value));
}

TEST(HostResolverInternalResultTest, ErrorResult) {
  auto result = std::make_unique<HostResolverInternalErrorResult>(
      "domain3.test", DnsQueryType::PTR, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kUnknown, ERR_NAME_NOT_RESOLVED);

  EXPECT_EQ(result->domain_name(), "domain3.test");
  EXPECT_EQ(result->query_type(), DnsQueryType::PTR);
  EXPECT_EQ(result->type(), HostResolverInternalResult::Type::kError);
  EXPECT_EQ(result->source(), HostResolverInternalResult::Source::kUnknown);
  EXPECT_THAT(result->expiration(), Optional(base::TimeTicks()));
  EXPECT_THAT(result->timed_expiration(), Optional(base::Time()));

  EXPECT_THAT(result->AsError(), Ref(*result));

  EXPECT_EQ(result->error(), ERR_NAME_NOT_RESOLVED);
}

TEST(HostResolverInternalResultTest, CloneErrorResult) {
  auto result = std::make_unique<HostResolverInternalErrorResult>(
      "domain3.test", DnsQueryType::PTR, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kUnknown, ERR_NAME_NOT_RESOLVED);

  std::unique_ptr<HostResolverInternalResult> copy = result->Clone();
  EXPECT_NE(copy.get(), result.get());

  EXPECT_EQ(copy->domain_name(), "domain3.test");
  EXPECT_EQ(copy->query_type(), DnsQueryType::PTR);
  EXPECT_EQ(copy->type(), HostResolverInternalResult::Type::kError);
  EXPECT_EQ(copy->source(), HostResolverInternalResult::Source::kUnknown);
  EXPECT_THAT(copy->expiration(), Optional(base::TimeTicks()));
  EXPECT_THAT(copy->timed_expiration(), Optional(base::Time()));
  EXPECT_EQ(copy->AsError().error(), ERR_NAME_NOT_RESOLVED);
}

TEST(HostResolverInternalResultTest, NoncachableErrorResult) {
  auto result = std::make_unique<HostResolverInternalErrorResult>(
      "domain3.test", DnsQueryType::PTR, /*expiration=*/std::nullopt,
      /*timed_expiration=*/std::nullopt,
      HostResolverInternalResult::Source::kUnknown, ERR_NAME_NOT_RESOLVED);

  EXPECT_EQ(result->domain_name(), "domain3.test");
  EXPECT_EQ(result->query_type(), DnsQueryType::PTR);
  EXPECT_EQ(result->type(), HostResolverInternalResult::Type::kError);
  EXPECT_EQ(result->source(), HostResolverInternalResult::Source::kUnknown);
  EXPECT_FALSE(result->expiration().has_value());
  EXPECT_FALSE(result->timed_expiration().has_value());

  EXPECT_THAT(result->AsError(), Ref(*result));

  EXPECT_EQ(result->error(), ERR_NAME_NOT_RESOLVED);
}

TEST(HostResolverInternalResultTest, RoundtripErrorResultThroughSerialization) {
  auto result = std::make_unique<HostResolverInternalErrorResult>(
      "domain4.test", DnsQueryType::A, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns, ERR_DNS_SERVER_FAILED);

  base::Value value = result->ToValue();
  auto deserialized = HostResolverInternalResult::FromValue(value);
  ASSERT_TRUE(deserialized);
  ASSERT_EQ(deserialized->type(), HostResolverInternalResult::Type::kError);

  // Expect deserialized result to be the same as the original other than
  // missing non-timed expiration.
  EXPECT_EQ(deserialized->AsError(),
            HostResolverInternalErrorResult(
                result->domain_name(), result->query_type(),
                /*expiration=*/std::nullopt, result->timed_expiration().value(),
                result->source(), result->error()));
}

// Expect results to serialize to a consistent base::Value format for
// consumption by NetLog and similar.
TEST(HostResolverInternalResultTest, SerializepErrorResult) {
  auto result = std::make_unique<HostResolverInternalErrorResult>(
      "domain4.test", DnsQueryType::A, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns, ERR_DNS_SERVER_FAILED);
  base::Value value = result->ToValue();

  std::optional<base::Value> expected = base::JSONReader::Read(
      R"(
        {
          "domain_name": "domain4.test",
          "error": -802,
          "query_type": "A",
          "source": "dns",
          "timed_expiration": "0",
          "type": "error"
        }
        )");
  ASSERT_TRUE(expected.has_value());

  EXPECT_EQ(value, expected.value());
}

TEST(HostResolverInternalResultTest, DeserializeMalformedErrorValue) {
  auto result = std::make_unique<HostResolverInternalErrorResult>(
      "domain4.test", DnsQueryType::A, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns, ERR_DNS_SERVER_FAILED);
  base::Value valid_value = result->ToValue();
  ASSERT_TRUE(HostResolverInternalErrorResult::FromValue(valid_value));

  base::Value missing_domain = valid_value.Clone();
  ASSERT_TRUE(missing_domain.GetDict().Remove("domain_name"));
  EXPECT_FALSE(HostResolverInternalErrorResult::FromValue(missing_domain));

  base::Value missing_qtype = valid_value.Clone();
  ASSERT_TRUE(missing_qtype.GetDict().Remove("query_type"));
  EXPECT_FALSE(HostResolverInternalErrorResult::FromValue(missing_qtype));
  base::Value unknown_qtype = valid_value.Clone();
  ASSERT_TRUE(unknown_qtype.GetDict().Set("query_type", "foo"));
  EXPECT_FALSE(HostResolverInternalErrorResult::FromValue(unknown_qtype));

  base::Value missing_value_type = valid_value.Clone();
  ASSERT_TRUE(missing_value_type.GetDict().Remove("type"));
  EXPECT_FALSE(HostResolverInternalErrorResult::FromValue(missing_value_type));
  base::Value unknown_value_type = valid_value.Clone();
  ASSERT_TRUE(unknown_value_type.GetDict().Set("type", "foo"));
  EXPECT_FALSE(HostResolverInternalErrorResult::FromValue(unknown_value_type));

  base::Value missing_source = valid_value.Clone();
  ASSERT_TRUE(missing_source.GetDict().Remove("source"));
  EXPECT_FALSE(HostResolverInternalErrorResult::FromValue(missing_source));
  base::Value unknown_source = valid_value.Clone();
  ASSERT_TRUE(unknown_source.GetDict().Set("source", "foo"));
  EXPECT_FALSE(HostResolverInternalErrorResult::FromValue(unknown_source));

  base::Value invalid_expiration = valid_value.Clone();
  ASSERT_TRUE(invalid_expiration.GetDict().Set("timed_expiration", "foo"));
  EXPECT_FALSE(HostResolverInternalErrorResult::FromValue(invalid_expiration));

  base::Value missing_error = valid_value.Clone();
  ASSERT_TRUE(missing_error.GetDict().Remove("error"));
  EXPECT_FALSE(HostResolverInternalErrorResult::FromValue(missing_error));
  base::Value invalid_error = valid_value.Clone();
  *invalid_error.GetDict().Find("error") = base::Value("foo");
  EXPECT_FALSE(HostResolverInternalErrorResult::FromValue(invalid_error));
}

TEST(HostResolverInternalResultTest, AliasResult) {
  auto result = std::make_unique<HostResolverInternalAliasResult>(
      "domain5.test", DnsQueryType::HTTPS, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns, "alias_target.test");

  EXPECT_EQ(result->domain_name(), "domain5.test");
  EXPECT_EQ(result->query_type(), DnsQueryType::HTTPS);
  EXPECT_EQ(result->type(), HostResolverInternalResult::Type::kAlias);
  EXPECT_EQ(result->source(), HostResolverInternalResult::Source::kDns);
  EXPECT_THAT(result->expiration(), Optional(base::TimeTicks()));
  EXPECT_THAT(result->timed_expiration(), Optional(base::Time()));

  EXPECT_THAT(result->AsAlias(), Ref(*result));

  EXPECT_THAT(result->alias_target(), "alias_target.test");
}

TEST(HostResolverInternalResultTest, CloneAliasResult) {
  auto result = std::make_unique<HostResolverInternalAliasResult>(
      "domain5.test", DnsQueryType::HTTPS, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns, "alias_target.test");

  std::unique_ptr<HostResolverInternalResult> copy = result->Clone();
  EXPECT_NE(copy.get(), result.get());

  EXPECT_EQ(copy->domain_name(), "domain5.test");
  EXPECT_EQ(copy->query_type(), DnsQueryType::HTTPS);
  EXPECT_EQ(copy->type(), HostResolverInternalResult::Type::kAlias);
  EXPECT_EQ(copy->source(), HostResolverInternalResult::Source::kDns);
  EXPECT_THAT(copy->expiration(), Optional(base::TimeTicks()));
  EXPECT_THAT(copy->timed_expiration(), Optional(base::Time()));
  EXPECT_THAT(copy->AsAlias().alias_target(), "alias_target.test");
}

TEST(HostResolverInternalResultTest, RoundtripAliasResultThroughSerialization) {
  auto result = std::make_unique<HostResolverInternalAliasResult>(
      "domain6.test", DnsQueryType::AAAA, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns, "alias_target1.test");

  base::Value value = result->ToValue();
  auto deserialized = HostResolverInternalResult::FromValue(value);
  ASSERT_TRUE(deserialized);
  ASSERT_EQ(deserialized->type(), HostResolverInternalResult::Type::kAlias);

  // Expect deserialized result to be the same as the original other than
  // missing non-timed expiration.
  EXPECT_EQ(deserialized->AsAlias(),
            HostResolverInternalAliasResult(
                result->domain_name(), result->query_type(),
                /*expiration=*/std::nullopt, result->timed_expiration().value(),
                result->source(), result->alias_target()));
}

// Expect results to serialize to a consistent base::Value format for
// consumption by NetLog and similar.
TEST(HostResolverInternalResultTest, SerializepAliasResult) {
  auto result = std::make_unique<HostResolverInternalAliasResult>(
      "domain6.test", DnsQueryType::AAAA, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns, "alias_target1.test");
  base::Value value = result->ToValue();

  std::optional<base::Value> expected = base::JSONReader::Read(
      R"(
        {
          "alias_target": "alias_target1.test",
          "domain_name": "domain6.test",
          "query_type": "AAAA",
          "source": "dns",
          "timed_expiration": "0",
          "type": "alias"
        }
        )");
  ASSERT_TRUE(expected.has_value());

  EXPECT_EQ(value, expected.value());
}

TEST(HostResolverInternalResultTest, DeserializeMalformedAliasValue) {
  auto result = std::make_unique<HostResolverInternalAliasResult>(
      "domain6.test", DnsQueryType::AAAA, base::TimeTicks(), base::Time(),
      HostResolverInternalResult::Source::kDns, "alias_target1.test");
  base::Value valid_value = result->ToValue();
  ASSERT_TRUE(HostResolverInternalAliasResult::FromValue(valid_value));

  base::Value missing_domain = valid_value.Clone();
  ASSERT_TRUE(missing_domain.GetDict().Remove("domain_name"));
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(missing_domain));

  base::Value missing_qtype = valid_value.Clone();
  ASSERT_TRUE(missing_qtype.GetDict().Remove("query_type"));
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(missing_qtype));
  base::Value unknown_qtype = valid_value.Clone();
  ASSERT_TRUE(unknown_qtype.GetDict().Set("query_type", "foo"));
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(unknown_qtype));

  base::Value missing_value_type = valid_value.Clone();
  ASSERT_TRUE(missing_value_type.GetDict().Remove("type"));
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(missing_value_type));
  base::Value unknown_value_type = valid_value.Clone();
  ASSERT_TRUE(unknown_value_type.GetDict().Set("type", "foo"));
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(unknown_value_type));

  base::Value missing_source = valid_value.Clone();
  ASSERT_TRUE(missing_source.GetDict().Remove("source"));
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(missing_source));
  base::Value unknown_source = valid_value.Clone();
  ASSERT_TRUE(unknown_source.GetDict().Set("source", "foo"));
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(unknown_source));

  base::Value missing_expiration = valid_value.Clone();
  ASSERT_TRUE(missing_expiration.GetDict().Remove("timed_expiration"));
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(missing_expiration));
  base::Value invalid_expiration = valid_value.Clone();
  ASSERT_TRUE(invalid_expiration.GetDict().Set("timed_expiration", "foo"));
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(invalid_expiration));

  base::Value missing_alias = valid_value.Clone();
  ASSERT_TRUE(missing_alias.GetDict().Remove("alias_target"));
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(missing_alias));
  base::Value invalid_alias = valid_value.Clone();
  *invalid_alias.GetDict().Find("alias_target") = base::Value(5);
  EXPECT_FALSE(HostResolverInternalAliasResult::FromValue(invalid_alias));
}

}  // namespace
}  // namespace net
