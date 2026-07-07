// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_pmt_report_util.h"

#include <optional>

#include "base/containers/span_writer.h"
#include "base/json/json_writer.h"
#include "base/strings/to_string.h"
#include "base/uuid.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/services/auction_worklet/public/cpp/private_model_training_reporting.h"
#include "crypto/hpke.h"
#include "crypto/keypair.h"

namespace content {

namespace {
// Helper to handle padding the payload and then framing it with it's length.
std::optional<std::vector<uint8_t>> FrameAndSerializePayload(
    mojo_base::BigBuffer payload,
    uint32_t desired_size) {
  // Header will only contain 4 bytes to represent the length
  const size_t kFramingHeaderSize = 4;
  std::vector<uint8_t> framed_payload(desired_size + kFramingHeaderSize, 0);

  // When the desired_size is greater than the allowed kMaxPayloadLength or
  // the payload size is greater than the desired_size, we will pass an empty
  // vector with a length of 0.
  if (payload.size() > desired_size) {
    return framed_payload;
  }

  base::SpanWriter<uint8_t> writer(framed_payload);

  // Set CBOR Payload Size Bytes
  if (!writer.WriteU32BigEndian(base::checked_cast<uint32_t>(payload.size()))) {
    return std::nullopt;
  }

  // Copy payload data
  if (!writer.Write(base::as_byte_span(payload))) {
    return std::nullopt;
  }
  return framed_payload;
}

// Handle encrypting the serialized payload with HPKE.
// Returns nullopt on error.
std::optional<std::vector<uint8_t>> EncryptPayloadWithHpke(
    std::vector<uint8_t> unencrypted_payload,
    uint32_t unused_desired_size,
    std::vector<uint8_t> encryption_shared_info,
    const BiddingAndAuctionServerKey& public_key) {
  std::optional<base::span<const uint8_t, 32>> public_key_bytes =
      base::as_byte_span(public_key.key).to_fixed_extent<32>();
  if (!public_key_bytes) {
    return std::nullopt;
  }

  std::optional<crypto::keypair::PublicKey> recipient =
      crypto::keypair::PublicKey::FromX25519PublicKey(*public_key_bytes);
  if (!recipient) {
    return std::nullopt;
  }
  const crypto::hpke::HpkeParams kParams = {
      .kem = crypto::hpke::KemType::kX25519HkdfSha256,
      .kdf = crypto::hpke::KdfType::kHkdfSha256,
      .aead = crypto::hpke::AeadType::kChaCha20Poly1305,
  };

  return crypto::hpke::Seal(kParams, *recipient,
                            /*plaintext=*/unencrypted_payload,
                            /*info=*/encryption_shared_info, /*ad=*/{});
}

}  // namespace

// PrivateModelTrainingRequest
PrivateModelTrainingRequest::PrivateModelTrainingRequest(
    auction_worklet::mojom::PrivateModelTrainingRequestDataPtr pmt_request_data,
    url::Origin reporting_origin,
    BiddingAndAuctionServerKey public_key)
    : PrivateModelTrainingRequest(std::move(pmt_request_data),
                                  std::move(reporting_origin),
                                  std::move(public_key),
                                  base::Uuid::GenerateRandomV4(),
                                  base::Time::Now()) {}

PrivateModelTrainingRequest::PrivateModelTrainingRequest(
    auction_worklet::mojom::PrivateModelTrainingRequestDataPtr pmt_request_data,
    url::Origin reporting_origin,
    BiddingAndAuctionServerKey public_key,
    base::Uuid report_id,
    base::Time scheduled_report_time)
    : shared_info_(std::move(report_id),
                   std::move(reporting_origin),
                   scheduled_report_time),
      pmt_request_data_(std::move(pmt_request_data)),
      public_key_(std::move(public_key)) {}

PrivateModelTrainingRequest::PrivateModelTrainingRequest(
    PrivateModelTrainingRequest&& other) = default;

PrivateModelTrainingRequest& PrivateModelTrainingRequest::operator=(
    PrivateModelTrainingRequest&& other) = default;

PrivateModelTrainingRequest::~PrivateModelTrainingRequest() = default;

// static
PrivateModelTrainingRequest
PrivateModelTrainingRequest::CreateRequestForTesting(
    auction_worklet::mojom::PrivateModelTrainingRequestDataPtr pmt_request_data,
    url::Origin reporting_origin,
    BiddingAndAuctionServerKey public_key,
    base::Uuid report_id,
    base::Time scheduled_report_time) {
  return PrivateModelTrainingRequest(
      std::move(pmt_request_data), std::move(reporting_origin),
      std::move(public_key), std::move(report_id),
      std::move(scheduled_report_time));
}

std::optional<std::vector<uint8_t>>
PrivateModelTrainingRequest::SerializeAndEncryptRequest() {
  // Ensure payload has not been moved.
  if (!pmt_request_data_->payload.data()) {
    return std::nullopt;
  }
  // Serialize and encrypt the payload
  const std::optional<std::vector<uint8_t>> serialized_payload =
      FrameAndSerializePayload(std::move(pmt_request_data_->payload),
                               pmt_request_data_->payload_length);
  if (!serialized_payload.has_value()) {
    return {};
  }

  std::optional<std::vector<uint8_t>> shared_info =
      GetSharedInfoCborWithPrefix();
  if (!shared_info.has_value()) {
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> encrypted_payload =
      EncryptPayloadWithHpke(std::move(serialized_payload.value()),
                             pmt_request_data_->payload_length,
                             std::move(shared_info.value()), public_key_);

  if (!encrypted_payload.has_value()) {
    return std::nullopt;
  }

  cbor::Value::MapValue aggregation_service_payload_cbor;

  aggregation_service_payload_cbor.emplace("payload",
                                           encrypted_payload.value());
  aggregation_service_payload_cbor.emplace("key_id", public_key_.id);

  cbor::Value::MapValue cbor_payload;
  cbor_payload.emplace("shared_info", GetSharedInfoCborMap());
  cbor_payload.emplace(
      "aggregation_coordinator_origin",
      pmt_request_data_->aggregation_coordinator_origin.spec());
  cbor_payload.emplace("aggregation_service_payload",
                       std::move(aggregation_service_payload_cbor));

  std::optional<std::vector<uint8_t>> final_cbor =
      cbor::Writer::Write(cbor::Value(std::move(cbor_payload)));

  return final_cbor;
}

std::optional<std::vector<uint8_t>>
PrivateModelTrainingRequest::GetSharedInfoCborWithPrefix() {
  cbor::Value::MapValue cbor_map = GetSharedInfoCborMap();

  std::optional<std::vector<uint8_t>> cbor_data =
      cbor::Writer::Write(cbor::Value(std::move(cbor_map)));
  if (!cbor_data.has_value()) {
    return std::nullopt;
  }
  std::vector<uint8_t> prefixed_cbor;
  prefixed_cbor.reserve(kDomainSeparationPrefix.size() +
                        cbor_data.value().size());

  prefixed_cbor.insert(prefixed_cbor.end(), kDomainSeparationPrefix.begin(),
                       kDomainSeparationPrefix.end());

  prefixed_cbor.insert(prefixed_cbor.end(), cbor_data.value().begin(),
                       cbor_data.value().end());
  return prefixed_cbor;
}

cbor::Value::MapValue PrivateModelTrainingRequest::GetSharedInfoCborMap() {
  cbor::Value::MapValue shared_info_cbor;
  shared_info_cbor.emplace("report_id",
                           shared_info_.report_id.AsLowercaseString());
  shared_info_cbor.emplace("reporting_origin",
                           shared_info_.reporting_origin.GetURL().spec());

  shared_info_cbor.emplace(
      "scheduled_report_time",
      shared_info_.scheduled_report_time.InMillisecondsSinceUnixEpoch());
  shared_info_cbor.emplace("version", shared_info_.version);
  shared_info_cbor.emplace("api", shared_info_.api);

  return shared_info_cbor;
}

// Shared Info
PrivateModelTrainingRequest::SharedInfo::SharedInfo(
    base::Uuid report_id,
    url::Origin reporting_origin,
    base::Time scheduled_report_time)
    : report_id(std::move(report_id)),
      reporting_origin(std::move(reporting_origin)),
      scheduled_report_time(scheduled_report_time) {}
PrivateModelTrainingRequest::SharedInfo::~SharedInfo() = default;
PrivateModelTrainingRequest::SharedInfo::SharedInfo(const SharedInfo&) =
    default;
PrivateModelTrainingRequest::SharedInfo::SharedInfo(SharedInfo&&) = default;

PrivateModelTrainingRequest::SharedInfo&
PrivateModelTrainingRequest::SharedInfo::operator=(const SharedInfo& other) =
    default;

PrivateModelTrainingRequest::SharedInfo&
PrivateModelTrainingRequest::SharedInfo::operator=(SharedInfo&& other) =
    default;

}  // namespace content
