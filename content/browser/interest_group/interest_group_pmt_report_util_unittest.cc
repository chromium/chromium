// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_pmt_report_util.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/browser/interest_group/interest_group_pmt_report_util.h"
#include "content/browser/interest_group/interest_group_pmt_test_utils.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/services/auction_worklet/public/cpp/private_model_training_reporting.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class InterestGroupPrivateModelTrainingUtilTest : public testing::Test {
 public:
  InterestGroupPrivateModelTrainingUtilTest() = default;

  ~InterestGroupPrivateModelTrainingUtilTest() override = default;
  // The size of the header which includes the length of the payload.
  const uint32_t kFramingHeaderSize = 4;
  const GURL kAggregationCoordinatorOrigin =
      GURL("https://aggregation-coordinator-origin.com");
  const GURL kDestination = GURL("https://destination.com");
  const url::Origin kReportingOrigin =
      url::Origin::Create(GURL("https://reporting-origin.com"));

  // This handles extracting the size of the cbor payload.
  // Should only be called with a framed cbor containing the length within the
  // first four bytes.
  uint32_t ExtractFramedSize(const std::vector<uint8_t>& framed_payload) {
    if (framed_payload.size() < 4) {
      return 0;
    }
    return (static_cast<uint32_t>(framed_payload[0]) << 24) |
           (static_cast<uint32_t>(framed_payload[1]) << 16) |
           (static_cast<uint32_t>(framed_payload[2]) << 8) |
           static_cast<uint32_t>(framed_payload[3]);
  }
};

// This test verifies that the encryption function works correctly, using pieces
// of a request to decrypt the payload.
TEST_F(InterestGroupPrivateModelTrainingUtilTest, EncryptAndDecryptPayload) {
  const std::vector<uint8_t> unencrypted_payload = {1, 2, 3, 4};
  const mojo_base::BigBuffer payload_big_buffer(unencrypted_payload);

  auction_worklet::mojom::PrivateModelTrainingRequestDataPtr payload_data =
      auction_worklet::mojom::PrivateModelTrainingRequestData::New(
          /*payload=*/payload_big_buffer.Clone(),
          /*payload_length*/ 256,
          /*aggregation_coordinator_origin=*/
          kAggregationCoordinatorOrigin,
          /*destination=*/kDestination);

  const auto scheduled_report_time = base::Time::Now();
  const base::Uuid report_id = base::Uuid::GenerateRandomV4();
  PrivateModelTrainingTestUtils::TestHpkeKey hpke_key;

  const BiddingAndAuctionServerKey public_key(hpke_key.GetPublicKey());

  // Create a request object to get SharedInfo as CBOR.
  PrivateModelTrainingRequest request =
      PrivateModelTrainingRequest::CreateRequestForTesting(
          std::move(payload_data), kReportingOrigin, public_key, report_id,
          scheduled_report_time);

  std::optional<std::vector<uint8_t>> cbor_data =
      request.SerializeAndEncryptRequest();
  ASSERT_TRUE(cbor_data.has_value());

  //  Decrypt the payload and verify its size and data are whats expected
  const std::optional<std::vector<uint8_t>> decrypted_payload =
      PrivateModelTrainingTestUtils::ExtractAndDecryptFramedPayloadFromCbor(
          cbor_data.value(), hpke_key.full_hpke_key());
  ASSERT_TRUE(decrypted_payload.has_value());
  // Extract the framing and build the actual payload based on the size.
  uint32_t framed_size = ExtractFramedSize(decrypted_payload.value());
  EXPECT_EQ(payload_big_buffer.size(), framed_size);
  ASSERT_GE(decrypted_payload.value().size(), kFramingHeaderSize + framed_size);
  std::vector<uint8_t> actual_payload(
      decrypted_payload.value().begin() + kFramingHeaderSize,
      decrypted_payload.value().begin() + kFramingHeaderSize + framed_size);

  EXPECT_EQ(unencrypted_payload, actual_payload);
}

// Ensures that when we serialize into cbor, we get what we expect.
// This verifies every field in the request, except the payload as that is done
// in a separate test.
TEST_F(InterestGroupPrivateModelTrainingUtilTest, SerializeRequestToCbor) {
  const uint32_t payload_length = 50;
  const std::vector<uint8_t> unencrypted_payload = {1, 2, 3, 4};
  const mojo_base::BigBuffer payload_big_buffer(unencrypted_payload);

  auction_worklet::mojom::PrivateModelTrainingRequestDataPtr payload_data =
      auction_worklet::mojom::PrivateModelTrainingRequestData::New(
          /*payload=*/payload_big_buffer.Clone(),
          /*payload_length*/ payload_length,
          /*aggregation_coordinator_origin=*/kAggregationCoordinatorOrigin,
          /*destination=*/kDestination);

  const auto scheduled_report_time = base::Time::Now();
  const base::Uuid report_id = base::Uuid::GenerateRandomV4();
  PrivateModelTrainingTestUtils::TestHpkeKey hpke_key;
  const BiddingAndAuctionServerKey public_key(hpke_key.GetPublicKey());

  PrivateModelTrainingRequest request =
      PrivateModelTrainingRequest::CreateRequestForTesting(
          std::move(payload_data), kReportingOrigin, public_key, report_id,
          scheduled_report_time);

  std::optional<std::vector<uint8_t>> cbor_data =
      request.SerializeAndEncryptRequest();
  ASSERT_TRUE(cbor_data.has_value());

  const auto maybe_map = cbor::Reader::Read(cbor_data.value());
  ASSERT_TRUE(maybe_map && maybe_map->is_map());
  const auto& map = maybe_map->GetMap();

  const auto aggregation_coordinator_origin_actual =
      map.find(cbor::Value("aggregation_coordinator_origin"));
  ASSERT_TRUE(aggregation_coordinator_origin_actual != map.end() &&
              aggregation_coordinator_origin_actual->second.is_string());
  EXPECT_EQ(aggregation_coordinator_origin_actual->second.GetString(),
            kAggregationCoordinatorOrigin.spec());

  // Extract and verify shared_info's cbor map
  const auto shared_info_it = map.find(cbor::Value("shared_info"));
  ASSERT_TRUE(shared_info_it != map.end() && shared_info_it->second.is_map());
  const auto& shared_info_map = shared_info_it->second.GetMap();

  const auto report_id_it = shared_info_map.find(cbor::Value("report_id"));
  ASSERT_TRUE(report_id_it != shared_info_map.end() &&
              report_id_it->second.is_string());
  EXPECT_FALSE(report_id_it->second.GetString().empty());
  EXPECT_EQ(report_id_it->second.GetString(), report_id.AsLowercaseString());

  const auto reporting_origin_it =
      shared_info_map.find(cbor::Value("reporting_origin"));
  ASSERT_TRUE(reporting_origin_it != shared_info_map.end() &&
              reporting_origin_it->second.is_string());
  EXPECT_EQ(reporting_origin_it->second.GetString(),
            kReportingOrigin.GetURL().spec());

  const auto api_it = shared_info_map.find(cbor::Value("api"));
  ASSERT_TRUE(api_it != shared_info_map.end() && api_it->second.is_string());
  EXPECT_EQ(api_it->second.GetString(), "private-model-training");

  const auto version_it = shared_info_map.find(cbor::Value("version"));
  ASSERT_TRUE(version_it != shared_info_map.end() &&
              version_it->second.is_string());
  EXPECT_EQ(version_it->second.GetString(), "1.0");

  const auto scheduled_report_time_it =
      shared_info_map.find(cbor::Value("scheduled_report_time"));
  ASSERT_TRUE(scheduled_report_time_it != shared_info_map.end() &&
              scheduled_report_time_it->second.is_integer());
  EXPECT_EQ(scheduled_report_time_it->second.GetInteger(),
            scheduled_report_time.InMillisecondsSinceUnixEpoch());

  // Extract and verify aggregation_service_payload's cbor map
  const auto aggregation_service_payload_it =
      map.find(cbor::Value("aggregation_service_payload"));
  ASSERT_TRUE(aggregation_service_payload_it != map.end() &&
              aggregation_service_payload_it->second.is_map());
  const auto& aggregation_service_payload_map =
      aggregation_service_payload_it->second.GetMap();

  const std::optional<std::vector<uint8_t>> framed_payload =
      PrivateModelTrainingTestUtils::ExtractAndDecryptFramedPayloadFromCbor(
          cbor_data.value(), hpke_key.full_hpke_key());
  ASSERT_TRUE(framed_payload.has_value());
  // Extract the framing and build the actual payload based on the size.
  uint32_t framed_size = ExtractFramedSize(framed_payload.value());
  EXPECT_EQ(payload_big_buffer.size(), framed_size);
  ASSERT_GE(framed_payload.value().size(), kFramingHeaderSize + framed_size);
  std::vector<uint8_t> actual_payload(
      framed_payload.value().begin() + kFramingHeaderSize,
      framed_payload.value().begin() + kFramingHeaderSize + framed_size);

  EXPECT_EQ(unencrypted_payload, actual_payload);

  // Extract and verify key_id
  const auto key_id_it =
      aggregation_service_payload_map.find(cbor::Value("key_id"));
  ASSERT_TRUE(key_id_it != aggregation_service_payload_map.end() &&
              key_id_it->second.is_string());
  EXPECT_EQ(key_id_it->second.GetString(), public_key.id);
}

// Verify that our SharedInfo as CBOR looks as expected.
TEST_F(InterestGroupPrivateModelTrainingUtilTest, SharedInfoCborWithPrefix) {
  const auto scheduled_report_time = base::Time::Now();
  const base::Uuid report_id = base::Uuid::GenerateRandomV4();
  const BiddingAndAuctionServerKey public_key("key", "id");

  auction_worklet::mojom::PrivateModelTrainingRequestDataPtr payload_data =
      auction_worklet::mojom::PrivateModelTrainingRequestData::New(
          /*payload=*/mojo_base::BigBuffer(),
          /*payload_length*/ 256,
          /*aggregation_coordinator_origin=*/
          kAggregationCoordinatorOrigin,
          /*destination=*/kDestination);

  PrivateModelTrainingRequest request =
      PrivateModelTrainingRequest::CreateRequestForTesting(
          std::move(payload_data), kReportingOrigin, public_key, report_id,
          scheduled_report_time);

  // Get the shared info as CBOR with prefix
  std::optional<std::vector<uint8_t>> actual_prefixed_cbor =
      request.GetSharedInfoCborWithPrefix();
  ASSERT_TRUE(actual_prefixed_cbor.has_value());

  // Get the domain separation prefix as a byte vector
  std::vector<uint8_t> expected_prefix(
      PrivateModelTrainingRequest::kDomainSeparationPrefix.begin(),
      PrivateModelTrainingRequest::kDomainSeparationPrefix.end());
  // Extract the domain separator from the CBOR
  std::vector<uint8_t> actual_prefix(
      actual_prefixed_cbor.value().begin(),
      actual_prefixed_cbor.value().begin() + expected_prefix.size());

  // Check if the actual CBOR starts with the expected prefix
  EXPECT_EQ(actual_prefix, expected_prefix);

  // Check the rest of the CBOR data
  std::vector<uint8_t> actual_cbor_data(
      actual_prefixed_cbor.value().begin() + expected_prefix.size(),
      actual_prefixed_cbor.value().end());

  // Verify that the shared_info is exactly what we expect
  const auto maybe_map = cbor::Reader::Read(actual_cbor_data);
  ASSERT_TRUE(maybe_map && maybe_map->is_map());
  const auto& shared_info_map = maybe_map->GetMap();

  const auto report_id_it = shared_info_map.find(cbor::Value("report_id"));
  ASSERT_TRUE(report_id_it != shared_info_map.end() &&
              report_id_it->second.is_string());
  EXPECT_FALSE(report_id_it->second.GetString().empty());
  EXPECT_EQ(report_id_it->second.GetString(), report_id.AsLowercaseString());

  const auto reporting_origin_it =
      shared_info_map.find(cbor::Value("reporting_origin"));
  ASSERT_TRUE(reporting_origin_it != shared_info_map.end() &&
              reporting_origin_it->second.is_string());
  EXPECT_EQ(reporting_origin_it->second.GetString(),
            kReportingOrigin.GetURL().spec());

  const auto api_it = shared_info_map.find(cbor::Value("api"));
  ASSERT_TRUE(api_it != shared_info_map.end() && api_it->second.is_string());
  EXPECT_EQ(api_it->second.GetString(), "private-model-training");

  const auto version_it = shared_info_map.find(cbor::Value("version"));
  ASSERT_TRUE(version_it != shared_info_map.end() &&
              version_it->second.is_string());
  EXPECT_EQ(version_it->second.GetString(), "1.0");

  const auto scheduled_report_time_it =
      shared_info_map.find(cbor::Value("scheduled_report_time"));
  ASSERT_TRUE(scheduled_report_time_it != shared_info_map.end() &&
              scheduled_report_time_it->second.is_integer());
  EXPECT_EQ(scheduled_report_time_it->second.GetInteger(),
            scheduled_report_time.InMillisecondsSinceUnixEpoch());
}

// When a payload is smaller than the specified payload length we will add
// padding, and the convert it to cbor.
TEST_F(InterestGroupPrivateModelTrainingUtilTest,
       FrameAndSerializePayloadWithPadding) {
  // Create a payload smaller than the payload length.
  const uint32_t payload_length = 1024;
  const std::vector<uint8_t> expected_payload(payload_length - 100, 1);
  const mojo_base::BigBuffer expected_payload_buffer(expected_payload);

  auction_worklet::mojom::PrivateModelTrainingRequestDataPtr payload_data =
      auction_worklet::mojom::PrivateModelTrainingRequestData::New(
          /*payload=*/expected_payload_buffer.Clone(),
          /*payload_length*/ payload_length,
          /*aggregation_coordinator_origin=*/kAggregationCoordinatorOrigin,
          /*destination=*/kDestination);

  const auto scheduled_report_time = base::Time::Now();
  const base::Uuid report_id = base::Uuid::GenerateRandomV4();
  PrivateModelTrainingTestUtils::TestHpkeKey hpke_key;
  const BiddingAndAuctionServerKey public_key(hpke_key.GetPublicKey());

  PrivateModelTrainingRequest request =
      PrivateModelTrainingRequest::CreateRequestForTesting(
          std::move(payload_data), kReportingOrigin, public_key, report_id,
          scheduled_report_time);

  std::optional<std::vector<uint8_t>> cbor_data =
      request.SerializeAndEncryptRequest();
  ASSERT_TRUE(cbor_data.has_value());

  // Attempt to serialize the small payload.
  const std::optional<std::vector<uint8_t>> framed_payload =
      PrivateModelTrainingTestUtils::ExtractAndDecryptFramedPayloadFromCbor(
          cbor_data.value(), hpke_key.full_hpke_key());
  ASSERT_TRUE(framed_payload.has_value());
  // Verify that when we serialized, the payload was padded to the length (also
  // factor in the header size).
  EXPECT_EQ(framed_payload.value().size(), kFramingHeaderSize + payload_length);

  uint32_t framed_size = ExtractFramedSize(framed_payload.value());
  EXPECT_EQ(expected_payload_buffer.size(), framed_size);

  std::vector<uint8_t> actual_payload(
      framed_payload.value().begin() + kFramingHeaderSize,
      framed_payload.value().begin() + kFramingHeaderSize + framed_size);

  EXPECT_EQ(expected_payload, actual_payload);
}

// Verify that when the payload size matches the payload length, there is no
// padding and it works properly.
TEST_F(InterestGroupPrivateModelTrainingUtilTest,
       FrameAndSerializePayloadExactSize) {
  // Create a payload that is the same size as the payload length.
  uint32_t payload_length = 1024;
  std::vector<uint8_t> expected_payload(payload_length, 1);
  mojo_base::BigBuffer expected_payload_buffer(expected_payload);

  auction_worklet::mojom::PrivateModelTrainingRequestDataPtr payload_data =
      auction_worklet::mojom::PrivateModelTrainingRequestData::New(
          /*payload=*/expected_payload_buffer.Clone(),
          /*payload_length*/ payload_length,
          /*aggregation_coordinator_origin=*/kAggregationCoordinatorOrigin,
          /*destination=*/kDestination);

  const auto scheduled_report_time = base::Time::Now();
  const base::Uuid report_id = base::Uuid::GenerateRandomV4();
  PrivateModelTrainingTestUtils::TestHpkeKey hpke_key;
  const BiddingAndAuctionServerKey public_key(hpke_key.GetPublicKey());

  PrivateModelTrainingRequest request =
      PrivateModelTrainingRequest::CreateRequestForTesting(
          std::move(payload_data), kReportingOrigin, public_key, report_id,
          scheduled_report_time);

  std::optional<std::vector<uint8_t>> cbor_data =
      request.SerializeAndEncryptRequest();
  ASSERT_TRUE(cbor_data.has_value());

  // Attempt to serialize the payload.
  const std::optional<std::vector<uint8_t>> framed_payload =
      PrivateModelTrainingTestUtils::ExtractAndDecryptFramedPayloadFromCbor(
          cbor_data.value(), hpke_key.full_hpke_key());
  ASSERT_TRUE(framed_payload.has_value());
  // Verify that when we serialized, the payload was padded to the length (also
  // factor in the header size).
  EXPECT_EQ(framed_payload.value().size(), kFramingHeaderSize + payload_length);

  uint32_t framed_size = ExtractFramedSize(framed_payload.value());
  EXPECT_EQ(expected_payload_buffer.size(), framed_size);

  std::vector<uint8_t> actual_payload(
      framed_payload.value().begin() + kFramingHeaderSize,
      framed_payload.value().begin() + kFramingHeaderSize + framed_size);

  EXPECT_EQ(expected_payload, actual_payload);
}

// When a payload is larger than the specified payload length we will make it
// length 0 (padding only).
TEST_F(InterestGroupPrivateModelTrainingUtilTest,
       FrameAndSerializePayloadTooLarge) {
  // Create a payload larger than the payload length.
  uint32_t payload_length = 1024;
  std::vector<uint8_t> expected_payload(payload_length + 1, 1);
  mojo_base::BigBuffer expected_payload_buffer(expected_payload);

  auction_worklet::mojom::PrivateModelTrainingRequestDataPtr payload_data =
      auction_worklet::mojom::PrivateModelTrainingRequestData::New(
          /*payload=*/expected_payload_buffer.Clone(),
          /*payload_length*/ payload_length,
          /*aggregation_coordinator_origin=*/kAggregationCoordinatorOrigin,
          /*destination=*/kDestination);

  const auto scheduled_report_time = base::Time::Now();
  const base::Uuid report_id = base::Uuid::GenerateRandomV4();
  PrivateModelTrainingTestUtils::TestHpkeKey hpke_key;
  const BiddingAndAuctionServerKey public_key(hpke_key.GetPublicKey());

  PrivateModelTrainingRequest request =
      PrivateModelTrainingRequest::CreateRequestForTesting(
          std::move(payload_data), kReportingOrigin, public_key, report_id,
          scheduled_report_time);

  std::optional<std::vector<uint8_t>> cbor_data =
      request.SerializeAndEncryptRequest();
  ASSERT_TRUE(cbor_data.has_value());

  // Attempt to serialize the large payload.
  const std::optional<std::vector<uint8_t>> framed_payload =
      PrivateModelTrainingTestUtils::ExtractAndDecryptFramedPayloadFromCbor(
          cbor_data.value(), hpke_key.full_hpke_key());
  ASSERT_TRUE(framed_payload.has_value());
  // Verify that when we serialized, the payload was padded to the length (also
  // factor in the header size).
  EXPECT_EQ(framed_payload.value().size(), kFramingHeaderSize + payload_length);

  EXPECT_EQ(ExtractFramedSize(framed_payload.value()), 0);
}

}  // namespace content
