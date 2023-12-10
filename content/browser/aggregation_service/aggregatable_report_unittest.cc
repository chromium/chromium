// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/json/json_writer.h"
#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/aggregation_service/features.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

testing::AssertionResult CborMapContainsKeyAndType(
    const cbor::Value::MapValue& map,
    const std::string& key,
    cbor::Value::Type value_type) {
  const auto it = map.find(cbor::Value(key));
  if (it == map.end()) {
    return testing::AssertionFailure()
           << "Expected key cbor::Value(\"" << key << "\") to be in map";
  }

  if (it->second.type() != value_type) {
    return testing::AssertionFailure()
           << "Expected value to have type " << static_cast<int>(value_type)
           << ", actual: " << static_cast<int>(it->second.type());
  }

  return testing::AssertionSuccess();
}

std::vector<blink::mojom::AggregatableReportHistogramContribution>
PadContributions(
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions,
    int max_contributions_allowed) {
  EXPECT_LE(static_cast<int>(contributions.size()), max_contributions_allowed);
  for (int i = contributions.size(); i < max_contributions_allowed; ++i) {
    contributions.emplace_back(/*bucket=*/0, /*value=*/0);
  }
  return contributions;
}

// Tests that the report has the expected format, matches the provided details,
// and is decryptable by the provided keys. Note that
// `expected_payload_contents` is not expected to have its contributions already
// padded.
void VerifyReport(
    const std::optional<AggregatableReport>& report,
    const AggregationServicePayloadContents& expected_payload_contents,
    const AggregatableReportSharedInfo& expected_shared_info,
    size_t expected_num_processing_urls,
    const std::optional<uint64_t>& expected_debug_key,
    const base::flat_map<std::string, std::string>& expected_additional_fields,
    const std::vector<aggregation_service::TestHpkeKey>& encryption_keys,
    bool should_pad_contributions) {
  ASSERT_TRUE(report.has_value());

  std::string expected_serialized_shared_info =
      expected_shared_info.SerializeAsJson();
  EXPECT_EQ(report->shared_info(), expected_serialized_shared_info);

  EXPECT_EQ(report->debug_key(), expected_debug_key);
  EXPECT_EQ(report->additional_fields(), expected_additional_fields);

  const std::vector<AggregatableReport::AggregationServicePayload>& payloads =
      report->payloads();
  ASSERT_EQ(payloads.size(), expected_num_processing_urls);
  ASSERT_EQ(encryption_keys.size(), expected_num_processing_urls);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_contributions;
  if (should_pad_contributions) {
    expected_contributions =
        PadContributions(expected_payload_contents.contributions,
                         expected_payload_contents.max_contributions_allowed);
  } else {
    expected_contributions = expected_payload_contents.contributions;
  }

  for (size_t i = 0; i < expected_num_processing_urls; ++i) {
    EXPECT_EQ(payloads[i].key_id, encryption_keys[i].key_id());

    std::vector<uint8_t> decrypted_payload =
        aggregation_service::DecryptPayloadWithHpke(
            payloads[i].payload, encryption_keys[i].full_hpke_key(),
            expected_serialized_shared_info);
    ASSERT_FALSE(decrypted_payload.empty());

    if (expected_shared_info.debug_mode ==
        AggregatableReportSharedInfo::DebugMode::kEnabled) {
      ASSERT_TRUE(payloads[i].debug_cleartext_payload.has_value());
      EXPECT_EQ(payloads[i].debug_cleartext_payload.value(), decrypted_payload);
    } else {
      EXPECT_FALSE(payloads[i].debug_cleartext_payload.has_value());
    }

    std::optional<cbor::Value> deserialized_payload =
        cbor::Reader::Read(decrypted_payload);
    ASSERT_TRUE(deserialized_payload.has_value());
    ASSERT_TRUE(deserialized_payload->is_map());
    const cbor::Value::MapValue& payload_map = deserialized_payload->GetMap();

    EXPECT_EQ(payload_map.size(), 2UL);

    ASSERT_TRUE(CborMapContainsKeyAndType(payload_map, "operation",
                                          cbor::Value::Type::STRING));
    EXPECT_EQ(payload_map.at(cbor::Value("operation")).GetString(),
              "histogram");

    switch (expected_payload_contents.aggregation_mode) {
      case blink::mojom::AggregationServiceMode::kTeeBased: {
        ASSERT_TRUE(CborMapContainsKeyAndType(payload_map, "data",
                                              cbor::Value::Type::ARRAY));
        const cbor::Value::ArrayValue& data_array =
            payload_map.at(cbor::Value("data")).GetArray();

        ASSERT_EQ(data_array.size(), expected_contributions.size());
        for (size_t j = 0; j < data_array.size(); ++j) {
          ASSERT_TRUE(data_array[j].is_map());
          const cbor::Value::MapValue& data_map = data_array[j].GetMap();

          ASSERT_TRUE(CborMapContainsKeyAndType(
              data_map, "bucket", cbor::Value::Type::BYTE_STRING));
          const cbor::Value::BinaryValue& bucket_byte_string =
              data_map.at(cbor::Value("bucket")).GetBytestring();
          EXPECT_EQ(bucket_byte_string.size(), 16u);  // 16 bytes = 128 bits

          // TODO(crbug.com/1298196): Replace with `base::ReadBigEndian()` when
          // available.
          absl::uint128 bucket;
          base::HexStringToUInt128(base::HexEncode(bucket_byte_string),
                                   &bucket);
          EXPECT_EQ(bucket, expected_contributions[j].bucket);

          ASSERT_TRUE(CborMapContainsKeyAndType(
              data_map, "value", cbor::Value::Type::BYTE_STRING));
          const cbor::Value::BinaryValue& value_byte_string =
              data_map.at(cbor::Value("value")).GetBytestring();
          EXPECT_EQ(value_byte_string.size(), 4u);  // 4 bytes = 32 bits

          // TODO(crbug.com/1298196): Replace with `base::ReadBigEndian()` when
          // available.
          uint32_t value;
          base::HexStringToUInt(base::HexEncode(value_byte_string), &value);
          EXPECT_EQ(static_cast<int64_t>(value),
                    expected_contributions[j].value);
        }

        EXPECT_FALSE(payload_map.contains(cbor::Value("dpf_key")));
        break;
      }
      case blink::mojom::AggregationServiceMode::kExperimentalPoplar: {
        EXPECT_TRUE(CborMapContainsKeyAndType(payload_map, "dpf_key",
                                              cbor::Value::Type::BYTE_STRING));

        // TODO(crbug.com/1238459): Test the payload details (e.g. dpf key) in
        // more depth against a minimal helper server implementation.

        EXPECT_FALSE(payload_map.contains(cbor::Value("data")));
        break;
      }
    }
  }
}

class AggregatableReportTest : public ::testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{kPrivacySandboxAggregationServiceReportPadding,
                                 {}},
                                {::aggregation_service::
                                     kAggregationServiceMultipleCloudProviders,
                                 {{"aws_cloud", "https://aws.example.test"},
                                  {"gcp_cloud", "https://gcp.example.test"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{::aggregation_service::
                                     kAggregationServiceMultipleCloudProviders,
                                 {{"aws_cloud", "https://aws.example.test"},
                                  {"gcp_cloud", "https://gcp.example.test"}}}},
          /*disabled_features=*/{
              kPrivacySandboxAggregationServiceReportPadding});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(AggregatableReportTest,
       ValidExperimentalPoplarRequest_ValidReportReturned) {
  AggregatableReportRequest request = aggregation_service::CreateExampleRequest(
      blink::mojom::AggregationServiceMode::kExperimentalPoplar);

  AggregationServicePayloadContents expected_payload_contents =
      request.payload_contents();
  AggregatableReportSharedInfo expected_shared_info =
      request.shared_info().Clone();
  size_t expected_num_processing_urls = request.processing_urls().size();

  std::vector<aggregation_service::TestHpkeKey> hpke_keys;
  hpke_keys.emplace_back("id123");
  hpke_keys.emplace_back("456abc");

  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          std::move(request),
          {hpke_keys[0].GetPublicKey(), hpke_keys[1].GetPublicKey()});

  ASSERT_NO_FATAL_FAILURE(
      VerifyReport(report, expected_payload_contents, expected_shared_info,
                   expected_num_processing_urls,
                   /*expected_debug_key=*/std::nullopt,
                   /*expected_additional_fields=*/{}, std::move(hpke_keys),
                   /*should_pad_contributions=*/GetParam()));
}

TEST_P(AggregatableReportTest, ValidTeeBasedRequest_ValidReportReturned) {
  AggregatableReportRequest request = aggregation_service::CreateExampleRequest(
      blink::mojom::AggregationServiceMode::kTeeBased);

  AggregationServicePayloadContents expected_payload_contents =
      request.payload_contents();
  AggregatableReportSharedInfo expected_shared_info =
      request.shared_info().Clone();
  size_t expected_num_processing_urls = request.processing_urls().size();

  std::vector<aggregation_service::TestHpkeKey> hpke_keys;
  hpke_keys.emplace_back("id123");

  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          std::move(request), {hpke_keys[0].GetPublicKey()});

  ASSERT_NO_FATAL_FAILURE(
      VerifyReport(report, expected_payload_contents, expected_shared_info,
                   expected_num_processing_urls,
                   /*expected_debug_key=*/std::nullopt,
                   /*expected_additional_fields=*/{}, std::move(hpke_keys),
                   /*should_pad_contributions=*/GetParam()));
}

TEST_P(AggregatableReportTest,
       ValidMultipleContributionsRequest_ValidReportReturned) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest(
          blink::mojom::AggregationServiceMode::kTeeBased);

  AggregationServicePayloadContents expected_payload_contents =
      example_request.payload_contents();
  expected_payload_contents.contributions = {
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/456),
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/7890,
          /*value=*/1234)};

  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(expected_payload_contents,
                                        example_request.shared_info().Clone());
  ASSERT_TRUE(request.has_value());

  AggregatableReportSharedInfo expected_shared_info =
      request->shared_info().Clone();
  size_t expected_num_processing_urls = request->processing_urls().size();

  std::vector<aggregation_service::TestHpkeKey> hpke_keys;
  hpke_keys.emplace_back("id123");

  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          std::move(*request), {hpke_keys[0].GetPublicKey()});

  ASSERT_NO_FATAL_FAILURE(
      VerifyReport(report, expected_payload_contents, expected_shared_info,
                   expected_num_processing_urls,
                   /*expected_debug_key=*/std::nullopt,
                   /*expected_additional_fields=*/{}, std::move(hpke_keys),
                   /*should_pad_contributions=*/GetParam()));
}

TEST_P(AggregatableReportTest,
       ValidNoContributionsRequest_ValidReportReturned) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest(
          blink::mojom::AggregationServiceMode::kTeeBased);

  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.contributions.clear();

  AggregationServicePayloadContents expected_payload_contents =
      payload_contents;
  if (!GetParam()) {
    // A null contribution should be added automatically.
    expected_payload_contents.contributions = {
        blink::mojom::AggregatableReportHistogramContribution(
            /*bucket=*/0,
            /*value=*/0)};
  }

  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone());
  ASSERT_TRUE(request.has_value());

  AggregatableReportSharedInfo expected_shared_info =
      request->shared_info().Clone();
  size_t expected_num_processing_urls = request->processing_urls().size();

  std::vector<aggregation_service::TestHpkeKey> hpke_keys;
  hpke_keys.emplace_back("id123");

  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          std::move(*request), {hpke_keys[0].GetPublicKey()});

  ASSERT_NO_FATAL_FAILURE(
      VerifyReport(report, expected_payload_contents, expected_shared_info,
                   expected_num_processing_urls,
                   /*expected_debug_key=*/std::nullopt,
                   /*expected_additional_fields=*/{}, std::move(hpke_keys),
                   /*should_pad_contributions=*/GetParam()));
}

TEST_P(AggregatableReportTest,
       ValidDebugModeEnabledRequest_ValidReportReturned) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.debug_mode =
      AggregatableReportSharedInfo::DebugMode::kEnabled;
  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(example_request.payload_contents(),
                                        expected_shared_info.Clone());
  ASSERT_TRUE(request.has_value());

  AggregationServicePayloadContents expected_payload_contents =
      request->payload_contents();
  size_t expected_num_processing_urls = request->processing_urls().size();

  std::vector<aggregation_service::TestHpkeKey> hpke_keys;
  hpke_keys.emplace_back("id123");

  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          std::move(request.value()), {hpke_keys[0].GetPublicKey()});

  ASSERT_NO_FATAL_FAILURE(
      VerifyReport(report, expected_payload_contents, expected_shared_info,
                   expected_num_processing_urls,
                   /*expected_debug_key=*/std::nullopt,
                   /*expected_additional_fields=*/{}, std::move(hpke_keys),
                   /*should_pad_contributions=*/GetParam()));
}

TEST_P(AggregatableReportTest,
       ValidDebugKeyPresentRequest_ValidReportReturned) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.debug_mode =
      AggregatableReportSharedInfo::DebugMode::kEnabled;

  // Use a large value to check that higher order bits are serialized too.
  uint64_t expected_debug_key = std::numeric_limits<uint64_t>::max() - 1;
  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), expected_shared_info.Clone(),
          /*reporting_path=*/std::string(), expected_debug_key);
  ASSERT_TRUE(request.has_value());

  AggregationServicePayloadContents expected_payload_contents =
      request->payload_contents();
  size_t expected_num_processing_urls = request->processing_urls().size();

  std::vector<aggregation_service::TestHpkeKey> hpke_keys;
  hpke_keys.emplace_back("id123");

  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          std::move(request.value()), {hpke_keys[0].GetPublicKey()});

  ASSERT_NO_FATAL_FAILURE(
      VerifyReport(report, expected_payload_contents, expected_shared_info,
                   expected_num_processing_urls, expected_debug_key,
                   /*expected_additional_fields=*/{}, std::move(hpke_keys),
                   /*should_pad_contributions=*/GetParam()));
}

TEST_P(AggregatableReportTest, AdditionalFieldsPresent_ValidReportReturned) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  base::flat_map<std::string, std::string> expected_additional_fields = {
      {"additional_key", "example_value"}, {"second", "field"}, {"", ""}};
  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(example_request.payload_contents(),
                                        example_request.shared_info().Clone(),
                                        /*reporting_path=*/std::string(),
                                        /*debug_key=*/std::nullopt,
                                        expected_additional_fields);
  ASSERT_TRUE(request.has_value());

  AggregationServicePayloadContents expected_payload_contents =
      request->payload_contents();
  size_t expected_num_processing_urls = request->processing_urls().size();

  std::vector<aggregation_service::TestHpkeKey> hpke_keys;
  hpke_keys.emplace_back("id123");

  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          std::move(request.value()), {hpke_keys[0].GetPublicKey()});

  ASSERT_NO_FATAL_FAILURE(VerifyReport(
      report, expected_payload_contents, example_request.shared_info(),
      expected_num_processing_urls, /*expected_debug_key=*/std::nullopt,
      expected_additional_fields, std::move(hpke_keys),
      /*should_pad_contributions=*/GetParam()));
}

TEST_P(AggregatableReportTest,
       RequestCreatedWithNonPositiveValue_FailsIfNegative) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();

  AggregationServicePayloadContents zero_value_payload_contents =
      payload_contents;
  zero_value_payload_contents.contributions[0].value = 0;
  std::optional<AggregatableReportRequest> zero_value_request =
      AggregatableReportRequest::Create(zero_value_payload_contents,
                                        shared_info.Clone());
  EXPECT_TRUE(zero_value_request.has_value());

  AggregationServicePayloadContents negative_value_payload_contents =
      payload_contents;
  negative_value_payload_contents.contributions[0].value = -1;
  std::optional<AggregatableReportRequest> negative_value_request =
      AggregatableReportRequest::Create(negative_value_payload_contents,
                                        shared_info.Clone());
  EXPECT_FALSE(negative_value_request.has_value());
}

TEST_P(AggregatableReportTest, RequestCreatedWithInvalidReportId_Failed) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();
  shared_info.report_id = base::Uuid();

  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(example_request.payload_contents(),
                                        std::move(shared_info));

  EXPECT_FALSE(request.has_value());
}

TEST_P(AggregatableReportTest, TeeBasedRequestCreatedWithZeroContributions) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest(
          blink::mojom::AggregationServiceMode::kTeeBased);

  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();

  payload_contents.contributions.clear();
  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone());
  EXPECT_TRUE(request.has_value());
}

TEST_P(AggregatableReportTest,
       ExperimentalPoplarRequestNotCreatedWithZeroContributions) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest(
          blink::mojom::AggregationServiceMode::kExperimentalPoplar);

  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();

  payload_contents.contributions.clear();
  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone());
  EXPECT_FALSE(request.has_value());
}

TEST_P(AggregatableReportTest, RequestCreatedWithTooManyContributions) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest(
          blink::mojom::AggregationServiceMode::kExperimentalPoplar);

  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.contributions = {
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/456),
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/7890,
          /*value=*/1234)};

  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone());
  ASSERT_FALSE(request.has_value());
}

TEST_P(AggregatableReportTest,
       RequestCreatedWithDebugKeyButDebugModeDisabled_Failed) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(example_request.payload_contents(),
                                        example_request.shared_info().Clone(),
                                        /*reporting_path=*/std::string(),
                                        /*debug_key=*/1234);

  EXPECT_FALSE(request.has_value());
}

TEST_P(AggregatableReportTest, GetAsJsonOnePayload_ValidJsonReturned) {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/std::nullopt);

  AggregatableReport report(std::move(payloads), "example_shared_info",
                            /*debug_key=*/std::nullopt,
                            /*additional_fields=*/{},
                            /*aggregation_coordinator_origin=*/std::nullopt);

  std::string report_json_string;
  base::JSONWriter::Write(base::Value(report.GetAsJson()), &report_json_string);

  const char kExpectedJsonString[] =
      R"({)"
      R"("aggregation_coordinator_origin":"https://aws.example.test",)"
      R"("aggregation_service_payloads":[)"
      R"({"key_id":"key_1","payload":"ABCD1234"})"
      R"(],)"
      R"("shared_info":"example_shared_info")"
      R"(})";
  EXPECT_EQ(report_json_string, kExpectedJsonString);
}

TEST_P(AggregatableReportTest, GetAsJsonTwoPayloads_ValidJsonReturned) {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/std::nullopt);
  payloads.emplace_back(/*payload=*/kEFGH5678AsBytes,
                        /*key_id=*/"key_2",
                        /*debug_cleartext_payload=*/std::nullopt);

  AggregatableReport report(std::move(payloads), "example_shared_info",
                            /*debug_key=*/std::nullopt,
                            /*additional_fields=*/{},
                            /*aggregation_coordinator_origin=*/std::nullopt);

  std::string report_json_string;
  base::JSONWriter::Write(base::Value(report.GetAsJson()), &report_json_string);

  const char kExpectedJsonString[] =
      R"({)"
      R"("aggregation_coordinator_origin":"https://aws.example.test",)"
      R"("aggregation_service_payloads":[)"
      R"({"key_id":"key_1","payload":"ABCD1234"},)"
      R"({"key_id":"key_2","payload":"EFGH5678"})"
      R"(],)"
      R"("shared_info":"example_shared_info")"
      R"(})";
  EXPECT_EQ(report_json_string, kExpectedJsonString);
}

TEST_P(AggregatableReportTest,
       GetAsJsonDebugCleartextPayload_ValidJsonReturned) {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/kEFGH5678AsBytes);

  AggregatableReport report(std::move(payloads), "example_shared_info",
                            /*debug_key=*/std::nullopt,
                            /*additional_fields=*/{},
                            /*aggregation_coordinator_origin=*/std::nullopt);

  std::string report_json_string;
  base::JSONWriter::Write(base::Value(report.GetAsJson()), &report_json_string);

  const char kExpectedJsonString[] =
      R"({)"
      R"("aggregation_coordinator_origin":"https://aws.example.test",)"
      R"("aggregation_service_payloads":[{)"
      R"("debug_cleartext_payload":"EFGH5678",)"
      R"("key_id":"key_1",)"
      R"("payload":"ABCD1234")"
      R"(}],)"
      R"("shared_info":"example_shared_info")"
      R"(})";
  EXPECT_EQ(report_json_string, kExpectedJsonString);
}

TEST_P(AggregatableReportTest, GetAsJsonDebugKey_ValidJsonReturned) {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/kEFGH5678AsBytes);

  AggregatableReport report(std::move(payloads), "example_shared_info",
                            /*debug_key=*/1234, /*additional_fields=*/{},
                            /*aggregation_coordinator_origin=*/std::nullopt);

  std::string report_json_string;
  base::JSONWriter::Write(base::Value(report.GetAsJson()), &report_json_string);

  const char kExpectedJsonString[] =
      R"({)"
      R"("aggregation_coordinator_origin":"https://aws.example.test",)"
      R"("aggregation_service_payloads":[{)"
      R"("debug_cleartext_payload":"EFGH5678",)"
      R"("key_id":"key_1",)"
      R"("payload":"ABCD1234")"
      R"(}],)"
      R"("debug_key":"1234",)"
      R"("shared_info":"example_shared_info")"
      R"(})";
  EXPECT_EQ(report_json_string, kExpectedJsonString);
}

TEST_P(AggregatableReportTest, GetAsJsonAdditionalFields_ValidJsonReturned) {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/std::nullopt);

  AggregatableReport report(
      std::move(payloads), "example_shared_info",
      /*debug_key=*/std::nullopt, /*additional_fields=*/
      {{"additional_key", "example_value"}, {"second", "field"}, {"", ""}},
      /*aggregation_coordinator_origin=*/std::nullopt);

  std::string report_json_string;
  base::JSONWriter::Write(base::Value(report.GetAsJson()), &report_json_string);

  const char kExpectedJsonString[] =
      R"({)"
      R"("":"",)"
      R"("additional_key":"example_value",)"
      R"("aggregation_coordinator_origin":"https://aws.example.test",)"
      R"("aggregation_service_payloads":[{)"
      R"("key_id":"key_1",)"
      R"("payload":"ABCD1234")"
      R"(}],)"
      R"("second":"field",)"
      R"("shared_info":"example_shared_info")"
      R"(})";
  EXPECT_EQ(report_json_string, kExpectedJsonString);
}

TEST_P(AggregatableReportTest,
       SharedInfoDebugModeDisabled_SerializeAsJsonReturnsExpectedString) {
  AggregatableReportSharedInfo shared_info(
      base::Time::FromMillisecondsSinceUnixEpoch(1234567890123),
      /*report_id=*/
      base::Uuid::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e"),
      url::Origin::Create(GURL("https://reporting.example")),
      AggregatableReportSharedInfo::DebugMode::kDisabled, base::Value::Dict(),
      /*api_version=*/"1.0",
      /*api_identifier=*/"example-api");

  const char kExpectedString[] =
      R"({)"
      R"("api":"example-api",)"
      R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
      R"("reporting_origin":"https://reporting.example",)"
      R"("scheduled_report_time":"1234567890",)"
      R"("version":"1.0")"
      R"(})";

  EXPECT_EQ(shared_info.SerializeAsJson(), kExpectedString);
}

TEST_P(AggregatableReportTest,
       SharedInfoDebugModeEnabled_SerializeAsJsonReturnsExpectedString) {
  AggregatableReportSharedInfo shared_info(
      base::Time::FromMillisecondsSinceUnixEpoch(1234567890123),
      /*report_id=*/
      base::Uuid::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e"),
      url::Origin::Create(GURL("https://reporting.example")),
      AggregatableReportSharedInfo::DebugMode::kEnabled, base::Value::Dict(),
      /*api_version=*/"1.0",
      /*api_identifier=*/"example-api");

  const char kExpectedString[] =
      R"({)"
      R"("api":"example-api",)"
      R"("debug_mode":"enabled",)"
      R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
      R"("reporting_origin":"https://reporting.example",)"
      R"("scheduled_report_time":"1234567890",)"
      R"("version":"1.0")"
      R"(})";

  EXPECT_EQ(shared_info.SerializeAsJson(), kExpectedString);
}

TEST_P(AggregatableReportTest, SharedInfoAdditionalFields) {
  base::Value::Dict additional_fields;
  additional_fields.Set("foo", "1");
  additional_fields.Set("bar", "2");
  additional_fields.Set("baz", "3");
  AggregatableReportSharedInfo shared_info(
      base::Time::FromMillisecondsSinceUnixEpoch(1234567890123),
      /*report_id=*/
      base::Uuid::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e"),
      url::Origin::Create(GURL("https://reporting.example")),
      AggregatableReportSharedInfo::DebugMode::kEnabled,
      std::move(additional_fields),
      /*api_version=*/"1.0",
      /*api_identifier=*/"example-api");

  const char kExpectedString[] =
      R"({)"
      R"("api":"example-api",)"
      R"("bar":"2",)"
      R"("baz":"3",)"
      R"("debug_mode":"enabled",)"
      R"("foo":"1",)"
      R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
      R"("reporting_origin":"https://reporting.example",)"
      R"("scheduled_report_time":"1234567890",)"
      R"("version":"1.0")"
      R"(})";

  EXPECT_EQ(shared_info.SerializeAsJson(), kExpectedString);
}

TEST_P(AggregatableReportTest, ReportingPathSet_SetInRequest) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest(
          blink::mojom::AggregationServiceMode::kExperimentalPoplar);

  std::string reporting_path = "/example-path";

  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(example_request.payload_contents(),
                                        example_request.shared_info().Clone(),
                                        reporting_path);
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->reporting_path(), reporting_path);
  EXPECT_EQ(request->GetReportingUrl().path(), reporting_path);
  EXPECT_EQ(request->GetReportingUrl().GetWithEmptyPath(),
            example_request.shared_info().reporting_origin.GetURL());
}

TEST_P(AggregatableReportTest, RequestCreatedWithInvalidFailedAttempt_Failed) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();

  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(shared_info),
          /*reporting_path=*/"", /*debug_key=*/std::nullopt,
          /*additional_fields=*/{},
          /*failed_send_attempts=*/-1);

  EXPECT_FALSE(request.has_value());
}

TEST_P(AggregatableReportTest,
       RequestCreatedWithMaxContributionsAllowed_FailsIfInvalid) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();

  payload_contents.max_contributions_allowed = -1;

  std::optional<AggregatableReportRequest> negative_request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone());
  EXPECT_FALSE(negative_request.has_value());

  payload_contents.contributions.emplace_back(/*bucket=*/456,
                                              /*value=*/78);
  payload_contents.max_contributions_allowed = 1;

  std::optional<AggregatableReportRequest> too_small_max_request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone());
  EXPECT_FALSE(too_small_max_request.has_value());

  payload_contents.contributions = {};
  payload_contents.max_contributions_allowed = 0;

  std::optional<AggregatableReportRequest> empty_zero_request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone());
  EXPECT_TRUE(empty_zero_request.has_value());
}

TEST_P(AggregatableReportTest, FailedSendAttempts) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  // Requests are initialized with no failed attempts by default
  EXPECT_EQ(example_request.failed_send_attempts(), 0);

  AggregatableReportRequest example_request_with_failed_attempts =
      aggregation_service::CreateExampleRequest(
          /*aggregation_mode=*/blink::mojom::AggregationServiceMode::kDefault,
          /*failed_send_attempts=*/2);

  // The failed attempts are correctly serialized & deserialized
  std::vector<uint8_t> proto = example_request_with_failed_attempts.Serialize();
  std::optional<AggregatableReportRequest> parsed_request =
      AggregatableReportRequest::Deserialize(proto);
  EXPECT_EQ(parsed_request.value().failed_send_attempts(), 2);
}

TEST_P(AggregatableReportTest, MaxContributionsAllowed) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.max_contributions_allowed = 20;

  AggregatableReportRequest request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  // The max contributions allowed is correctly serialized and deserialized
  std::vector<uint8_t> proto = request.Serialize();
  std::optional<AggregatableReportRequest> parsed_request =
      AggregatableReportRequest::Deserialize(proto);
  EXPECT_EQ(parsed_request.value().payload_contents().max_contributions_allowed,
            20);
}

TEST_P(AggregatableReportTest, AggregationCoordinatorOrigin) {
  const struct {
    std::optional<url::Origin> aggregation_coordinator_origin;
    bool creation_should_succeed;
    const char* description;
  } kTestCases[] = {
      {std::nullopt, true, "default coordinator"},
      {url::Origin::Create(GURL("https://aws.example.test")), true,
       "valid coordinator"},
      {url::Origin::Create(GURL("https://a.test")), false,
       "invalid coordinator"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    AggregatableReportRequest example_request =
        aggregation_service::CreateExampleRequest();

    AggregationServicePayloadContents payload_contents =
        example_request.payload_contents();
    payload_contents.aggregation_coordinator_origin =
        test_case.aggregation_coordinator_origin;

    std::optional<AggregatableReportRequest> request =
        AggregatableReportRequest::Create(
            payload_contents, example_request.shared_info().Clone());

    EXPECT_EQ(request.has_value(), test_case.creation_should_succeed);

    if (!request.has_value()) {
      continue;
    }

    // The coordinator origin is correctly serialized and deserialized
    std::vector<uint8_t> proto = request->Serialize();
    std::optional<AggregatableReportRequest> parsed_request =
        AggregatableReportRequest::Deserialize(proto);
    EXPECT_EQ(parsed_request.value()
                  .payload_contents()
                  .aggregation_coordinator_origin,
              test_case.aggregation_coordinator_origin);
  }
}

TEST_P(AggregatableReportTest, AggregationCoordinatorOriginAllowlistChanged) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      ::aggregation_service::kAggregationServiceMultipleCloudProviders,
      {{"aws_cloud", "https://aws.example.test"},
       {"gcp_cloud", "https://gcp.example.test"}});

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.aggregation_coordinator_origin =
      url::Origin::Create(GURL("https://aws.example.test"));

  AggregatableReportRequest request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  std::vector<uint8_t> proto = request.Serialize();

  // Change the allowlist between serializing and deserializing
  scoped_feature_list.Reset();
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      ::aggregation_service::kAggregationServiceMultipleCloudProviders,
      {{"aws_cloud", "https://aws2.example.test"},
       {"gcp_cloud", "https://gcp2.example.test"}});

  // Expect the report to fail to be recreated.
  std::optional<AggregatableReportRequest> parsed_request =
      AggregatableReportRequest::Deserialize(proto);
  EXPECT_FALSE(parsed_request.has_value());
}

TEST_P(AggregatableReportTest, ReportingPathEmpty_NotSetInRequest) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest(
          blink::mojom::AggregationServiceMode::kExperimentalPoplar);

  std::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(example_request.payload_contents(),
                                        example_request.shared_info().Clone());
  ASSERT_TRUE(request.has_value());
  EXPECT_TRUE(request->reporting_path().empty());

  // If the reporting path is empty,
  EXPECT_FALSE(request->GetReportingUrl().is_valid());
}

TEST_P(AggregatableReportTest, EmptyPayloads) {
  AggregatableReport report(/*payloads=*/{}, "example_shared_info",
                            /*debug_key=*/std::nullopt,
                            /*additional_fields=*/{},
                            /*aggregation_coordinator_origin=*/std::nullopt);

  std::string report_json_string;
  base::JSONWriter::Write(base::Value(report.GetAsJson()), &report_json_string);

  const char kExpectedJsonString[] =
      R"({)"
      R"("aggregation_coordinator_origin":"https://aws.example.test",)"
      R"("shared_info":"example_shared_info")"
      R"(})";
  EXPECT_EQ(report_json_string, kExpectedJsonString);
}

TEST(AggregatableReportProtoMigrationTest,
     NoDebugKeyOrFailedSendAttempts_ParsesCorrectly) {
  // An `AggregatableReport` serialized before the addition of the `debug_key`
  // field and `failed_send_attempts` field.
  const char kHexEncodedOldProto[] =
      "0A071205107B18C803126208D0DA8693FDBECF17122431323334353637382D393061622D"
      "346364652D386631322D3334353637383930616263641A1368747470733A2F2F6578616D"
      "706C652E636F6D2A0F6578616D706C652D76657273696F6E320B6578616D706C652D6170"
      "691A0C6578616D706C652D70617468";

  std::vector<uint8_t> old_proto;
  EXPECT_TRUE(base::HexStringToBytes(kHexEncodedOldProto, &old_proto));

  std::optional<AggregatableReportRequest> deserialized_request =
      AggregatableReportRequest::Deserialize(old_proto);
  ASSERT_TRUE(deserialized_request.has_value());

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(
          AggregationServicePayloadContents(
              AggregationServicePayloadContents::Operation::kHistogram,
              {blink::mojom::AggregatableReportHistogramContribution(
                  /*bucket=*/123, /*value=*/456)},
              blink::mojom::AggregationServiceMode::kDefault,
              /*aggregation_coordinator_origin=*/std::nullopt,
              /*max_contributions_allowed=*/1),
          AggregatableReportSharedInfo(
              base::Time::FromMillisecondsSinceUnixEpoch(1652984901234),
              base::Uuid::ParseLowercase(
                  "12345678-90ab-4cde-8f12-34567890abcd"),
              /*reporting_origin=*/
              url::Origin::Create(GURL("https://example.com")),
              AggregatableReportSharedInfo::DebugMode::kDisabled,
              /*additional_fields=*/base::Value::Dict(),
              /*api_version=*/"example-version",
              /*api_identifier=*/"example-api"),
          /*reporting_path=*/"example-path", /*debug_key=*/std::nullopt,
          /*additional_fields=*/{},
          /*failed_send_attempts=*/0)
          .value();

  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      deserialized_request.value(), expected_request));
}

TEST(AggregatableReportProtoMigrationTest, NegativeDebugKey_ParsesCorrectly) {
  // An `AggregatableReport` serialized while `debug_key` was stored as a signed
  // int64 and used a value that was larger than the maximum int64. It was
  // therefore stored as a negative number.
  const char kHexEncodedOldProto[] =
      "0A071205107B18C803126408D0DA8693FDBECF17122431323334353637382D393061622D"
      "346364652D386631322D3334353637383930616263641A1368747470733A2F2F6578616D"
      "706C652E636F6D20012A0F6578616D706C652D76657273696F6E320B6578616D706C652D"
      "6170691A0C6578616D706C652D7061746820FFFFFFFFFFFFFFFFFF01";

  std::vector<uint8_t> old_proto;
  EXPECT_TRUE(base::HexStringToBytes(kHexEncodedOldProto, &old_proto));

  std::optional<AggregatableReportRequest> deserialized_request =
      AggregatableReportRequest::Deserialize(old_proto);
  ASSERT_TRUE(deserialized_request.has_value());

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(
          AggregationServicePayloadContents(
              AggregationServicePayloadContents::Operation::kHistogram,
              {blink::mojom::AggregatableReportHistogramContribution(
                  /*bucket=*/123, /*value=*/456)},
              blink::mojom::AggregationServiceMode::kDefault,
              /*aggregation_coordinator_origin=*/std::nullopt,
              /*max_contributions_allowed=*/1),
          AggregatableReportSharedInfo(
              base::Time::FromMillisecondsSinceUnixEpoch(1652984901234),
              base::Uuid::ParseLowercase(
                  "12345678-90ab-4cde-8f12-34567890abcd"),
              /*reporting_origin=*/
              url::Origin::Create(GURL("https://example.com")),
              AggregatableReportSharedInfo::DebugMode::kEnabled,
              /*additional_fields=*/base::Value::Dict(),
              /*api_version=*/"example-version",
              /*api_identifier=*/"example-api"),
          /*reporting_path=*/"example-path",
          /*debug_key=*/std::numeric_limits<uint64_t>::max())
          .value();

  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      deserialized_request.value(), expected_request));
}

TEST(AggregatableReportProtoMigrationTest,
     NoAdditionalFieldsOrAggregationCoordinatorOrigin_ParsesCorrectly) {
  // An `AggregatableReport` serialized before `additional_fields` and
  // `aggregataion_coordinator_origin` were added to the proto definition.
  const char kHexEncodedOldProto[] =
      "0A071205107B18C803126208D0DA8693FDBECF17122431323334353637382D393061622D"
      "346364652D386631322D3334353637383930616263641A1368747470733A2F2F6578616D"
      "706C652E636F6D2A0F6578616D706C652D76657273696F6E320B6578616D706C652D6170"
      "691A0C6578616D706C652D70617468";

  std::vector<uint8_t> old_proto;
  EXPECT_TRUE(base::HexStringToBytes(kHexEncodedOldProto, &old_proto));

  std::optional<AggregatableReportRequest> deserialized_request =
      AggregatableReportRequest::Deserialize(old_proto);
  ASSERT_TRUE(deserialized_request.has_value());

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(
          AggregationServicePayloadContents(
              AggregationServicePayloadContents::Operation::kHistogram,
              {blink::mojom::AggregatableReportHistogramContribution(
                  /*bucket=*/123, /*value=*/456)},
              blink::mojom::AggregationServiceMode::kDefault,
              /*aggregation_coordinator_origin=*/std::nullopt,
              /*max_contributions_allowed=*/1),
          AggregatableReportSharedInfo(
              base::Time::FromMillisecondsSinceUnixEpoch(1652984901234),
              base::Uuid::ParseLowercase(
                  "12345678-90ab-4cde-8f12-34567890abcd"),
              /*reporting_origin=*/
              url::Origin::Create(GURL("https://example.com")),
              AggregatableReportSharedInfo::DebugMode::kDisabled,
              /*additional_fields=*/base::Value::Dict(),
              /*api_version=*/"example-version",
              /*api_identifier=*/"example-api"),
          /*reporting_path=*/"example-path", /*debug_key=*/std::nullopt,
          /*additional_fields=*/{},
          /*failed_send_attempts=*/0)
          .value();

  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      deserialized_request.value(), expected_request));
}

TEST_P(AggregatableReportTest, ProcessingUrlSet) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  EXPECT_THAT(
      request.processing_urls(),
      ::testing::ElementsAre(
          GetAggregationServiceProcessingUrl(url::Origin::Create(
              GURL(::aggregation_service::kAggregationServiceCoordinatorAwsCloud
                       .Get())))));
}

TEST_P(AggregatableReportTest, AggregationCoordinator_ProcessingUrlSet) {
  const struct {
    std::optional<url::Origin> aggregation_coordinator_origin;
    std::vector<GURL> expected_urls;
  } kTestCases[] = {
      {
          std::nullopt,
          {GURL("https://aws.example.test/.well-known/aggregation-service/v1/"
                "public-keys")},
      },
      {
          url::Origin::Create(GURL("https://aws.example.test")),
          {GURL("https://aws.example.test/.well-known/aggregation-service/v1/"
                "public-keys")},
      },
      {
          url::Origin::Create(GURL("https://gcp.example.test")),
          {GURL("https://gcp.example.test/.well-known/aggregation-service/v1/"
                "public-keys")},
      },
      {
          url::Origin::Create(GURL("https://a.test")),
          {},
      },
  };

  for (const auto& test_case : kTestCases) {
    std::optional<AggregatableReportRequest> request =
        AggregatableReportRequest::Create(
            AggregationServicePayloadContents(
                AggregationServicePayloadContents::Operation::kHistogram,
                {blink::mojom::AggregatableReportHistogramContribution(
                    /*bucket=*/123,
                    /*value=*/456)},
                blink::mojom::AggregationServiceMode::kDefault,
                test_case.aggregation_coordinator_origin,
                /*max_contributions_allowed=*/20),
            AggregatableReportSharedInfo(
                /*scheduled_report_time=*/base::Time::Now(),
                /*report_id=*/
                base::Uuid::GenerateRandomV4(),
                url::Origin::Create(GURL("https://reporting.example")),
                AggregatableReportSharedInfo::DebugMode::kDisabled,
                /*additional_fields=*/base::Value::Dict(),
                /*api_version=*/"",
                /*api_identifier=*/"example-api"),
            /*reporting_path=*/"example-path",
            /*debug_key=*/std::nullopt, /*additional_fields=*/{},
            /*failed_send_attempts=*/0);

    if (test_case.expected_urls.empty()) {
      EXPECT_FALSE(request.has_value());
    } else {
      EXPECT_EQ(request->processing_urls(), test_case.expected_urls);
    }
  }
}

TEST_P(AggregatableReportTest, AggregationCoordinator_SetInReport) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      ::aggregation_service::kAggregationServiceMultipleCloudProviders,
      {{"aws_cloud", "https://aws.example.test"}});

  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/std::nullopt);

  AggregatableReport report(std::move(payloads), "example_shared_info",
                            /*debug_key=*/std::nullopt,
                            /*additional_fields=*/{},
                            /*aggregation_coordinator_origin=*/std::nullopt);

  std::string report_json_string;
  base::JSONWriter::Write(base::Value(report.GetAsJson()), &report_json_string);

  const char kExpectedJsonString[] =
      R"({)"
      R"("aggregation_coordinator_origin":"https://aws.example.test",)"
      R"("aggregation_service_payloads":[)"
      R"({"key_id":"key_1","payload":"ABCD1234"})"
      R"(],)"
      R"("shared_info":"example_shared_info")"
      R"(})";
  EXPECT_EQ(report_json_string, kExpectedJsonString);
}

INSTANTIATE_TEST_SUITE_P(All, AggregatableReportTest, testing::Bool());

}  // namespace
}  // namespace content
