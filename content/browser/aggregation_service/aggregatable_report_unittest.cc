// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

std::vector<uint8_t> DecryptPayloadWithHpke(
    const std::vector<uint8_t>& payload,
    const EVP_HPKE_KEY& key,
    const std::string& expected_serialized_shared_info) {
  base::span<const uint8_t> enc =
      base::make_span(payload).subspan(0, X25519_PUBLIC_VALUE_LEN);

  std::vector<uint8_t> authenticated_info(
      AggregatableReport::kDomainSeparationPrefix,
      AggregatableReport::kDomainSeparationPrefix +
          sizeof(AggregatableReport::kDomainSeparationPrefix));
  authenticated_info.insert(authenticated_info.end(),
                            expected_serialized_shared_info.begin(),
                            expected_serialized_shared_info.end());

  bssl::ScopedEVP_HPKE_CTX recipient_context;
  if (!EVP_HPKE_CTX_setup_recipient(
          /*ctx=*/recipient_context.get(), /*key=*/&key,
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/EVP_hpke_chacha20_poly1305(),
          /*enc=*/enc.data(), /*enc_len=*/enc.size(),
          /*info=*/authenticated_info.data(),
          /*info_len=*/authenticated_info.size())) {
    return {};
  }

  base::span<const uint8_t> ciphertext =
      base::make_span(payload).subspan(X25519_PUBLIC_VALUE_LEN);
  std::vector<uint8_t> plaintext(ciphertext.size());
  size_t plaintext_len;

  if (!EVP_HPKE_CTX_open(
          /*ctx=*/recipient_context.get(), /*out=*/plaintext.data(),
          /*out_len*/ &plaintext_len, /*max_out_len=*/plaintext.size(),
          /*in=*/ciphertext.data(), /*in_len=*/ciphertext.size(),
          /*ad=*/nullptr,
          /*ad_len=*/0)) {
    return {};
  }

  plaintext.resize(plaintext_len);
  return plaintext;
}

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

// Tests that the report has the expected format, matches the provided details,
// and is decryptable by the provided keys.
void VerifyReport(
    const absl::optional<AggregatableReport>& report,
    const AggregationServicePayloadContents& expected_payload_contents,
    const AggregatableReportSharedInfo& expected_shared_info,
    const std::vector<url::Origin>& expected_processing_origins,
    const std::vector<aggregation_service::TestHpkeKey>& encryption_keys) {
  ASSERT_TRUE(report.has_value());

  std::string expected_serialized_shared_info =
      expected_shared_info.SerializeAsJson();
  EXPECT_EQ(report->shared_info(), expected_serialized_shared_info);

  const std::vector<AggregatableReport::AggregationServicePayload>& payloads =
      report->payloads();
  ASSERT_EQ(payloads.size(), expected_processing_origins.size());
  ASSERT_EQ(encryption_keys.size(), expected_processing_origins.size());

  for (size_t i = 0; i < expected_processing_origins.size(); ++i) {
    EXPECT_EQ(payloads[i].origin, expected_processing_origins[i]);
    EXPECT_EQ(payloads[i].key_id, encryption_keys[i].public_key.id);

    std::vector<uint8_t> decrypted_payload = DecryptPayloadWithHpke(
        payloads[i].payload, encryption_keys[i].full_hpke_key,
        expected_serialized_shared_info);
    ASSERT_FALSE(decrypted_payload.empty());

    absl::optional<cbor::Value> deserialized_payload =
        cbor::Reader::Read(decrypted_payload);
    ASSERT_TRUE(deserialized_payload.has_value());
    ASSERT_TRUE(deserialized_payload->is_map());
    const cbor::Value::MapValue& payload_map = deserialized_payload->GetMap();

    EXPECT_EQ(payload_map.size(), 3UL);

    ASSERT_TRUE(CborMapContainsKeyAndType(payload_map, "reporting_origin",
                                          cbor::Value::Type::STRING));
    EXPECT_EQ(url::Origin::Create(GURL(
                  payload_map.at(cbor::Value("reporting_origin")).GetString())),
              expected_payload_contents.reporting_origin);

    ASSERT_TRUE(CborMapContainsKeyAndType(payload_map, "operation",
                                          cbor::Value::Type::STRING));
    EXPECT_EQ(payload_map.at(cbor::Value("operation")).GetString(),
              "histogram");

    switch (expected_payload_contents.processing_type) {
      case AggregationServicePayloadContents::ProcessingType::kTwoParty: {
        EXPECT_TRUE(CborMapContainsKeyAndType(payload_map, "dpf_key",
                                              cbor::Value::Type::BYTE_STRING));

        // TODO(crbug.com/1238459): Test the payload details (e.g. dpf key) in
        // more depth against a minimal helper server implementation.

        EXPECT_FALSE(payload_map.contains(cbor::Value("data")));
        break;
      }
      case AggregationServicePayloadContents::ProcessingType::kSingleServer: {
        ASSERT_TRUE(CborMapContainsKeyAndType(payload_map, "data",
                                              cbor::Value::Type::ARRAY));
        const cbor::Value::ArrayValue& data_array =
            payload_map.at(cbor::Value("data")).GetArray();

        // TODO(crbug.com/1272030): Support multiple contributions in one
        // payload.
        EXPECT_EQ(data_array.size(), 1u);
        ASSERT_TRUE(data_array[0].is_map());
        const cbor::Value::MapValue& data_map = data_array[0].GetMap();

        ASSERT_TRUE(CborMapContainsKeyAndType(data_map, "bucket",
                                              cbor::Value::Type::UNSIGNED));
        EXPECT_EQ(data_map.at(cbor::Value("bucket")).GetInteger(),
                  expected_payload_contents.bucket);

        ASSERT_TRUE(CborMapContainsKeyAndType(data_map, "value",
                                              cbor::Value::Type::UNSIGNED));
        EXPECT_EQ(data_map.at(cbor::Value("value")).GetInteger(),
                  expected_payload_contents.value);

        EXPECT_FALSE(payload_map.contains(cbor::Value("dpf_key")));
        break;
      }
    }
  }
}

TEST(AggregatableReportTest, ValidTwoPartyRequest_ValidReportReturned) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  AggregationServicePayloadContents expected_payload_contents =
      request.payload_contents();
  AggregatableReportSharedInfo expected_shared_info = request.shared_info();
  std::vector<url::Origin> expected_processing_origins =
      request.processing_origins();
  std::vector<aggregation_service::TestHpkeKey> hpke_keys = {
      aggregation_service::GenerateKey("id123"),
      aggregation_service::GenerateKey("456abc")};

  absl::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          std::move(request),
          {hpke_keys[0].public_key, hpke_keys[1].public_key});

  ASSERT_NO_FATAL_FAILURE(VerifyReport(report, expected_payload_contents,
                                       expected_shared_info,
                                       expected_processing_origins, hpke_keys));
}

TEST(AggregatableReportTest, ValidSingleServerRequest_ValidReportReturned) {
  AggregatableReportRequest request = aggregation_service::CreateExampleRequest(
      AggregationServicePayloadContents::ProcessingType::kSingleServer);

  AggregationServicePayloadContents expected_payload_contents =
      request.payload_contents();
  AggregatableReportSharedInfo expected_shared_info = request.shared_info();
  std::vector<url::Origin> expected_processing_origins =
      request.processing_origins();

  aggregation_service::TestHpkeKey hpke_key =
      aggregation_service::GenerateKey("id123");

  absl::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          std::move(request), {hpke_key.public_key});

  ASSERT_NO_FATAL_FAILURE(
      VerifyReport(report, expected_payload_contents, expected_shared_info,
                   expected_processing_origins, {hpke_key}));
}

TEST(AggregatableReportTest,
     RequestCreatedWithNonPositiveBucketOrValue_FailsIfNegative) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  AggregatableReportSharedInfo shared_info = example_request.shared_info();

  AggregationServicePayloadContents zero_bucket_payload_contents =
      payload_contents;
  zero_bucket_payload_contents.bucket = 0;
  absl::optional<AggregatableReportRequest> zero_bucket_request =
      AggregatableReportRequest::Create(zero_bucket_payload_contents,
                                        shared_info);
  EXPECT_TRUE(zero_bucket_request.has_value());

  AggregationServicePayloadContents zero_value_payload_contents =
      payload_contents;
  zero_value_payload_contents.value = 0;
  absl::optional<AggregatableReportRequest> zero_value_request =
      AggregatableReportRequest::Create(zero_value_payload_contents,
                                        shared_info);
  EXPECT_TRUE(zero_value_request.has_value());

  AggregationServicePayloadContents negative_bucket_payload_contents =
      payload_contents;
  negative_bucket_payload_contents.bucket = -1;
  absl::optional<AggregatableReportRequest> negative_bucket_request =
      AggregatableReportRequest::Create(negative_bucket_payload_contents,
                                        shared_info);
  EXPECT_FALSE(negative_bucket_request.has_value());

  AggregationServicePayloadContents negative_value_payload_contents =
      payload_contents;
  negative_value_payload_contents.value = -1;
  absl::optional<AggregatableReportRequest> negative_value_request =
      AggregatableReportRequest::Create(negative_value_payload_contents,
                                        shared_info);
  EXPECT_FALSE(negative_value_request.has_value());
}

TEST(AggregatableReportTest, RequestCreatedWithInvalidReportId_Failed) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info = example_request.shared_info();
  shared_info.report_id = base::GUID();

  absl::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(example_request.payload_contents(),
                                        std::move(shared_info));

  EXPECT_FALSE(request.has_value());
}

TEST(AggregatableReportTest, GetAsJsonOnePayload_ValidJsonReturned) {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(url::Origin::Create(GURL("https://a.example")),
                        /*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1");

  AggregatableReport report(std::move(payloads), "example_shared_info");
  base::Value::DictStorage report_json_value = report.GetAsJson();

  std::string report_json_string;
  base::JSONWriter::Write(base::Value(report_json_value), &report_json_string);

  const char kExpectedJsonString[] =
      R"({)"
      R"("aggregation_service_payloads":[)"
      R"({"key_id":"key_1","origin":"https://a.example","payload":"ABCD1234"})"
      R"(],)"
      R"("shared_info":"example_shared_info")"
      R"(})";
  EXPECT_EQ(report_json_string, kExpectedJsonString);
}

TEST(AggregatableReportTest, GetAsJsonTwoPayloads_ValidJsonReturned) {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(url::Origin::Create(GURL("https://a.example")),
                        /*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1");
  payloads.emplace_back(url::Origin::Create(GURL("https://b.example")),
                        /*payload=*/kEFGH5678AsBytes,
                        /*key_id=*/"key_2");

  AggregatableReport report(std::move(payloads), "example_shared_info");
  base::Value::DictStorage report_json_value = report.GetAsJson();

  std::string report_json_string;
  base::JSONWriter::Write(base::Value(report_json_value), &report_json_string);

  const char kExpectedJsonString[] =
      R"({)"
      R"("aggregation_service_payloads":[)"
      R"({"key_id":"key_1","origin":"https://a.example","payload":"ABCD1234"},)"
      R"({"key_id":"key_2","origin":"https://b.example","payload":"EFGH5678"})"
      R"(],)"
      R"("shared_info":"example_shared_info")"
      R"(})";
  EXPECT_EQ(report_json_string, kExpectedJsonString);
}

TEST(AggregatableReportTest, SharedInfoSerializeAsJson_ReturnsExpectedString) {
  AggregatableReportSharedInfo shared_info(
      base::Time::FromJavaTime(1234567890123),
      /*privacy_budget_key=*/"example_pbk",
      /*report_id=*/
      base::GUID::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e"));

  const char kExpectedString[] =
      R"({)"
      R"("privacy_budget_key":"example_pbk",)"
      R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
      R"("scheduled_report_time":"1234567890",)"
      R"("version":"")"
      R"(})";

  EXPECT_EQ(shared_info.SerializeAsJson(), kExpectedString);
}

}  // namespace content
