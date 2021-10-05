// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
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

std::vector<uint8_t> DecryptPayloadWithHpke(const std::vector<uint8_t>& payload,
                                            const EVP_HPKE_KEY& key) {
  base::span<const uint8_t> enc =
      base::make_span(payload).subspan(0, X25519_PUBLIC_VALUE_LEN);

  std::vector<uint8_t> authenticated_info(
      AggregatableReport::kDomainSeparationValue,
      AggregatableReport::kDomainSeparationValue +
          sizeof(AggregatableReport::kDomainSeparationValue));

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

// Tests that the report has the expected format, matches the provided details,
// and is decryptable by the provided keys.
void ExpectReturnedValueMatchesReportDetails(
    const absl::optional<AggregatableReport>& report,
    const url::Origin& expected_reporting_origin,
    const AggregatableReportSharedInfo& expected_shared_info,
    const std::vector<url::Origin>& expected_processing_origins,
    const std::vector<aggregation_service::TestHpkeKey>& encryption_keys) {
  ASSERT_TRUE(report.has_value());

  EXPECT_TRUE(aggregation_service::SharedInfoEqual(report->shared_info(),
                                                   expected_shared_info));

  const std::vector<AggregatableReport::AggregationServicePayload>& payloads =
      report->payloads();
  EXPECT_EQ(payloads.size(), AggregatableReport::kNumberOfProcessingOrigins);
  EXPECT_EQ(expected_processing_origins.size(),
            AggregatableReport::kNumberOfProcessingOrigins);
  EXPECT_EQ(encryption_keys.size(),
            AggregatableReport::kNumberOfProcessingOrigins);

  for (size_t i = 0; i < AggregatableReport::kNumberOfProcessingOrigins; ++i) {
    EXPECT_EQ(payloads[i].origin, expected_processing_origins[i]);
    EXPECT_EQ(payloads[i].key_id, encryption_keys[i].public_key.id);

    std::vector<uint8_t> decrypted_payload = DecryptPayloadWithHpke(
        payloads[i].payload, encryption_keys[i].full_hpke_key);
    ASSERT_FALSE(decrypted_payload.empty());

    absl::optional<cbor::Value> deserialized_payload =
        cbor::Reader::Read(decrypted_payload);
    ASSERT_TRUE(deserialized_payload.has_value());
    ASSERT_TRUE(deserialized_payload->is_map());
    const cbor::Value::MapValue& payload_map = deserialized_payload->GetMap();

    EXPECT_EQ(payload_map.size(), 6UL);

    const auto version_it = payload_map.find(cbor::Value("version"));
    ASSERT_NE(version_it, payload_map.end());
    ASSERT_TRUE(version_it->second.is_string());
    EXPECT_EQ(version_it->second.GetString(), "");

    const auto reporting_origin_it =
        payload_map.find(cbor::Value("reporting_origin"));
    ASSERT_NE(reporting_origin_it, payload_map.end());
    ASSERT_TRUE(reporting_origin_it->second.is_string());
    EXPECT_EQ(
        url::Origin::Create(GURL(reporting_origin_it->second.GetString())),
        expected_reporting_origin);

    const auto privacy_budget_key_it =
        payload_map.find(cbor::Value("privacy_budget_key"));
    ASSERT_NE(privacy_budget_key_it, payload_map.end());
    ASSERT_TRUE(privacy_budget_key_it->second.is_string());
    EXPECT_EQ(privacy_budget_key_it->second.GetString(),
              expected_shared_info.privacy_budget_key);

    const auto scheduled_report_time_it =
        payload_map.find(cbor::Value("scheduled_report_time"));
    ASSERT_NE(scheduled_report_time_it, payload_map.end());
    ASSERT_TRUE(scheduled_report_time_it->second.is_integer());
    EXPECT_EQ(scheduled_report_time_it->second.GetInteger(),
              expected_shared_info.scheduled_report_time.ToJavaTime());

    const auto operation_it = payload_map.find(cbor::Value("operation"));
    ASSERT_NE(operation_it, payload_map.end());
    ASSERT_TRUE(operation_it->second.is_string());
    EXPECT_EQ(operation_it->second.GetString(), "hierarchical-histogram");

    const auto dpf_key_it = payload_map.find(cbor::Value("dpf_key"));
    ASSERT_NE(dpf_key_it, payload_map.end());
    ASSERT_TRUE(dpf_key_it->second.is_bytestring());

    // TODO(crbug.com/1238459): Test the payload details (e.g. dpf key) in more
    // depth against a minimal helper server implementation.
  }
}

TEST(AggregatableReportTest, ValidRequest_ValidReportReturned) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  url::Origin expected_reporting_origin =
      request.payload_contents().reporting_origin;
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

  ASSERT_NO_FATAL_FAILURE(ExpectReturnedValueMatchesReportDetails(
      report, expected_reporting_origin, expected_shared_info,
      expected_processing_origins, hpke_keys));
}

TEST(AggregatableReportTest, RequestCreated_RequiresRightNumberOfOrigins) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  AggregatableReportSharedInfo shared_info = example_request.shared_info();

  absl::optional<AggregatableReportRequest> zero_origins =
      AggregatableReportRequest::Create({}, payload_contents, shared_info);

  absl::optional<AggregatableReportRequest> one_origin =
      AggregatableReportRequest::Create(
          {url::Origin::Create(GURL("https://a.example"))}, payload_contents,
          shared_info);

  absl::optional<AggregatableReportRequest> two_origins =
      AggregatableReportRequest::Create(
          {url::Origin::Create(GURL("https://a.example")),
           url::Origin::Create(GURL("https://b.example"))},
          payload_contents, shared_info);

  absl::optional<AggregatableReportRequest> three_origins =
      AggregatableReportRequest::Create(
          {url::Origin::Create(GURL("https://a.example")),
           url::Origin::Create(GURL("https://b.example")),
           url::Origin::Create(GURL("https://c.example"))},
          payload_contents, shared_info);

  EXPECT_FALSE(zero_origins.has_value());
  EXPECT_FALSE(one_origin.has_value());
  EXPECT_TRUE(two_origins.has_value());
  EXPECT_FALSE(three_origins.has_value());
}

TEST(AggregatableReportTest,
     RequestCreatedWithSwappedOrigins_OrderingIsDeterminstic) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  AggregatableReportSharedInfo shared_info = example_request.shared_info();

  absl::optional<AggregatableReportRequest> ordering_1 =
      AggregatableReportRequest::Create(
          {url::Origin::Create(GURL("https://a.example")),
           url::Origin::Create(GURL("https://b.example"))},
          payload_contents, shared_info);

  absl::optional<AggregatableReportRequest> ordering_2 =
      AggregatableReportRequest::Create(
          {url::Origin::Create(GURL("https://b.example")),
           url::Origin::Create(GURL("https://a.example"))},
          payload_contents, shared_info);

  ASSERT_TRUE(ordering_1.has_value());
  ASSERT_TRUE(ordering_2.has_value());
  EXPECT_EQ(ordering_1->processing_origins(), ordering_2->processing_origins());
}

TEST(AggregatableReportTest, RequestCreatedWithInsecureOrigin_Failed) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  AggregatableReportSharedInfo shared_info = example_request.shared_info();

  absl::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(
          {url::Origin::Create(GURL("http://a.example")),
           url::Origin::Create(GURL("https://b.example"))},
          payload_contents, shared_info);

  EXPECT_FALSE(request.has_value());
}

TEST(AggregatableReportTest, RequestCreatedWithOpaqueOrigin_Failed) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  AggregatableReportSharedInfo shared_info = example_request.shared_info();

  absl::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create(
          {url::Origin::Create(GURL("about:blank")),
           url::Origin::Create(GURL("https://b.example"))},
          payload_contents, shared_info);

  EXPECT_FALSE(request.has_value());
}

TEST(AggregatableReportTest, GetAsJson_ValidJsonReturned) {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(url::Origin::Create(GURL("https://a.example")),
                        /*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1");
  payloads.emplace_back(url::Origin::Create(GURL("https://b.example")),
                        /*payload=*/kEFGH5678AsBytes,
                        /*key_id=*/"key_2");

  AggregatableReportSharedInfo shared_info(
      base::Time::FromJavaTime(1234567890123),
      /*privacy_budget_key=*/"example_pbk");

  AggregatableReport report(std::move(payloads), std::move(shared_info));
  base::Value::DictStorage report_json_value = std::move(report).GetAsJson();

  std::string report_json_string;
  base::JSONWriter::Write(base::Value(report_json_value), &report_json_string);

  const char kExpectedJsonString[] =
      R"({)"
      R"("aggregation_service_payloads":[)"
      R"({"key_id":"key_1","origin":"https://a.example","payload":"ABCD1234"},)"
      R"({"key_id":"key_2","origin":"https://b.example","payload":"EFGH5678"})"
      R"(],)"
      R"("privacy_budget_key":"example_pbk",)"
      R"("scheduled_report_time":"1234567890123","version":"")"
      R"(})";
  EXPECT_EQ(report_json_string, kExpectedJsonString);
}

}  // namespace content
