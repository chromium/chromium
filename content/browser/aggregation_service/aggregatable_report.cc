// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/json/json_string_value_serializer.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "third_party/distributed_point_functions/code/dpf/distributed_point_function.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using DistributedPointFunction =
    distributed_point_functions::DistributedPointFunction;
using DpfKey = distributed_point_functions::DpfKey;
using DpfParameters = distributed_point_functions::DpfParameters;

// Payload contents:
constexpr char kHistogramValue[] = "histogram";
constexpr char kOperationKey[] = "operation";

std::vector<url::Origin> GetDefaultProcessingOrigins(
    AggregationServicePayloadContents::ProcessingType processing_type) {
  switch (processing_type) {
    case AggregationServicePayloadContents::ProcessingType::kTwoParty:
      // TODO(crbug.com/1295705): Update default processing origins.
      return {url::Origin::Create(GURL("https://server1.example.com")),
              url::Origin::Create(GURL("https://server2.example.com"))};
    case AggregationServicePayloadContents::ProcessingType::kSingleServer:
      return {url::Origin::Create(GURL(
          features::kPrivacySandboxAggregationServiceTrustedServerOriginParam
              .Get()))};
  }
}

// Returns parameters that support each possible prefix length in
// `[1, kBucketDomainBitLength]` with the same element_bitsize of
// `kValueDomainBitLength`.
std::vector<DpfParameters> ConstructDpfParameters() {
  std::vector<DpfParameters> parameters(
      AggregatableReport::kBucketDomainBitLength);
  for (size_t i = 0; i < AggregatableReport::kBucketDomainBitLength; i++) {
    parameters[i].set_log_domain_size(i + 1);

    parameters[i].mutable_value_type()->mutable_integer()->set_bitsize(
        AggregatableReport::kValueDomainBitLength);
  }

  return parameters;
}

// Returns empty vector in case of error.
std::vector<DpfKey> GenerateDpfKeys(
    const AggregationServicePayloadContents& contents) {
  DCHECK_EQ(contents.operation,
            AggregationServicePayloadContents::Operation::kHistogram);
  DCHECK_EQ(contents.processing_type,
            AggregationServicePayloadContents::ProcessingType::kTwoParty);

  // absl::StatusOr is not allowed in the codebase, but this minimal usage is
  // necessary to interact with //third_party/distributed_point_functions/.
  absl::StatusOr<std::unique_ptr<DistributedPointFunction>> status_or_dpf =
      DistributedPointFunction::CreateIncremental(ConstructDpfParameters());
  if (!status_or_dpf.ok()) {
    return {};
  }
  std::unique_ptr<DistributedPointFunction> dpf =
      std::move(status_or_dpf).value();

  // We want the same beta, no matter which prefix length is used.
  absl::StatusOr<std::pair<DpfKey, DpfKey>> status_or_dpf_keys =
      dpf->GenerateKeysIncremental(
          /*alpha=*/contents.bucket,
          /*beta=*/std::vector<absl::uint128>(
              AggregatableReport::kBucketDomainBitLength, contents.value));
  if (!status_or_dpf_keys.ok()) {
    return {};
  }

  std::vector<DpfKey> dpf_keys;
  dpf_keys.push_back(std::move(status_or_dpf_keys->first));
  dpf_keys.push_back(std::move(status_or_dpf_keys->second));

  return dpf_keys;
}

// Returns a vector with a serialized CBOR map for each processing origin. See
// the AggregatableReport documentation for more detail on the expected format.
// Returns an empty vector in case of error.
std::vector<std::vector<uint8_t>> ConstructUnencryptedTwoPartyPayloads(
    const AggregationServicePayloadContents& payload_contents) {
  std::vector<DpfKey> dpf_keys = GenerateDpfKeys(payload_contents);
  if (dpf_keys.empty()) {
    return {};
  }
  DCHECK_EQ(dpf_keys.size(), 2u);

  std::vector<std::vector<uint8_t>> unencrypted_payloads;
  for (const DpfKey& dpf_key : dpf_keys) {
    std::vector<uint8_t> serialized_key(dpf_key.ByteSizeLong());
    bool succeeded =
        dpf_key.SerializeToArray(serialized_key.data(), serialized_key.size());
    DCHECK(succeeded);

    cbor::Value::MapValue value;
    value.emplace(kOperationKey, kHistogramValue);
    value.emplace("dpf_key", std::move(serialized_key));

    absl::optional<std::vector<uint8_t>> unencrypted_payload =
        cbor::Writer::Write(cbor::Value(std::move(value)));

    if (!unencrypted_payload.has_value()) {
      return {};
    }

    unencrypted_payloads.push_back(std::move(unencrypted_payload.value()));
  }

  return unencrypted_payloads;
}

// TODO(crbug.com/1298196): Replace with `base::WriteBigEndian()` when available
std::vector<uint8_t> EncodeBucketForPayload(absl::uint128 bucket) {
  std::vector<uint8_t> byte_string(sizeof(absl::uint128) / sizeof(uint8_t));

  // Construct the vector in reverse to ensure network byte (big-endian) order.
  for (auto it = byte_string.rbegin(); it != byte_string.rend(); ++it) {
    *it = static_cast<uint8_t>(bucket & 0xFF);
    bucket >>= 8;
  }
  DCHECK_EQ(bucket, 0);
  return byte_string;
}

// Returns a vector with a serialized CBOR map. See the AggregatableReport
// documentation for more detail on the expected format. Returns an empty
// vector in case of error.
// Note that a vector is returned to match the two party case.
std::vector<std::vector<uint8_t>> ConstructUnencryptedSingleServerPayload(
    const AggregationServicePayloadContents& payload_contents) {
  cbor::Value::MapValue value;
  value.emplace(kOperationKey, kHistogramValue);

  // TODO(crbug.com/1272030): Support multiple contributions in one payload.
  cbor::Value::ArrayValue data;
  cbor::Value::MapValue data_map;
  data_map.emplace("bucket", EncodeBucketForPayload(payload_contents.bucket));
  data_map.emplace("value", payload_contents.value);
  data.push_back(cbor::Value(std::move(data_map)));
  value.emplace("data", std::move(data));

  absl::optional<std::vector<uint8_t>> unencrypted_payload =
      cbor::Writer::Write(cbor::Value(std::move(value)));

  if (!unencrypted_payload.has_value()) {
    return {};
  }

  return {std::move(unencrypted_payload.value())};
}

// Encrypts the `plaintext` with HPKE using the processing origin's
// `public_key`. Returns empty vector if the encryption fails.
std::vector<uint8_t> EncryptWithHpke(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& public_key,
    const std::vector<uint8_t>& authenticated_info) {
  bssl::ScopedEVP_HPKE_CTX sender_context;

  // This vector will hold the encapsulated shared secret "enc" followed by the
  // symmetrically encrypted ciphertext "ct".
  std::vector<uint8_t> payload(EVP_HPKE_MAX_ENC_LENGTH);
  size_t encapsulated_shared_secret_len;

  DCHECK_EQ(public_key.size(), PublicKey::kKeyByteLength);

  if (!EVP_HPKE_CTX_setup_sender(
          /*ctx=*/sender_context.get(),
          /*out_enc=*/payload.data(),
          /*out_enc_len=*/&encapsulated_shared_secret_len,
          /*max_enc=*/payload.size(),
          /*kem=*/EVP_hpke_x25519_hkdf_sha256(), /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/EVP_hpke_chacha20_poly1305(),
          /*peer_public_key=*/public_key.data(),
          /*peer_public_key_len=*/public_key.size(),
          /*info=*/authenticated_info.data(),
          /*info_len=*/authenticated_info.size())) {
    return {};
  }

  payload.resize(encapsulated_shared_secret_len + plaintext.size() +
                 EVP_HPKE_CTX_max_overhead(sender_context.get()));

  base::span<uint8_t> ciphertext =
      base::make_span(payload).subspan(encapsulated_shared_secret_len);
  size_t ciphertext_len;

  if (!EVP_HPKE_CTX_seal(
          /*ctx=*/sender_context.get(), /*out=*/ciphertext.data(),
          /*out_len=*/&ciphertext_len,
          /*max_out_len=*/ciphertext.size(), /*in=*/plaintext.data(),
          /*in_len*/ plaintext.size(),
          /*ad=*/nullptr,
          /*ad_len=*/0)) {
    return {};
  }
  payload.resize(encapsulated_shared_secret_len + ciphertext_len);

  return payload;
}

}  // namespace

AggregationServicePayloadContents::AggregationServicePayloadContents(
    Operation operation,
    absl::uint128 bucket,
    int value,
    ProcessingType processing_type)
    : operation(operation),
      bucket(bucket),
      value(value),
      processing_type(processing_type) {}

AggregatableReportSharedInfo::AggregatableReportSharedInfo(
    base::Time scheduled_report_time,
    std::string privacy_budget_key,
    base::GUID report_id,
    url::Origin reporting_origin,
    DebugMode debug_mode)
    : scheduled_report_time(std::move(scheduled_report_time)),
      privacy_budget_key(std::move(privacy_budget_key)),
      report_id(std::move(report_id)),
      reporting_origin(std::move(reporting_origin)),
      debug_mode(debug_mode) {}

AggregatableReportSharedInfo::AggregatableReportSharedInfo(
    const AggregatableReportSharedInfo& other) = default;
AggregatableReportSharedInfo& AggregatableReportSharedInfo::operator=(
    const AggregatableReportSharedInfo& other) = default;
AggregatableReportSharedInfo::AggregatableReportSharedInfo(
    AggregatableReportSharedInfo&& other) = default;
AggregatableReportSharedInfo& AggregatableReportSharedInfo::operator=(
    AggregatableReportSharedInfo&& other) = default;
AggregatableReportSharedInfo::~AggregatableReportSharedInfo() = default;

std::string AggregatableReportSharedInfo::SerializeAsJson() const {
  base::Value value(base::Value::Type::DICTIONARY);

  value.SetStringKey("privacy_budget_key", privacy_budget_key);

  DCHECK(report_id.is_valid());
  value.SetStringKey("report_id", report_id.AsLowercaseString());

  value.SetStringKey("reporting_origin", reporting_origin.Serialize());

  // Encoded as the number of seconds since the Unix epoch, ignoring leap
  // seconds and rounded down.
  DCHECK(!scheduled_report_time.is_null());
  DCHECK(!scheduled_report_time.is_inf());
  value.SetStringKey("scheduled_report_time",
                     base::NumberToString(scheduled_report_time.ToJavaTime() /
                                          base::Time::kMillisecondsPerSecond));

  // TODO(alexmt): Replace with a real version once a version string is decided.
  value.SetStringKey("version", "");

  // Only include the field if enabled.
  if (debug_mode == DebugMode::kEnabled) {
    value.SetStringKey("debug_mode", "enabled");
  }

  std::string serialized_value;
  JSONStringValueSerializer serializer(&serialized_value);
  bool succeeded = serializer.Serialize(value);
  DCHECK(succeeded);

  return serialized_value;
}

// static
absl::optional<AggregatableReportRequest> AggregatableReportRequest::Create(
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info) {
  std::vector<url::Origin> processing_origins =
      GetDefaultProcessingOrigins(payload_contents.processing_type);
  return CreateInternal(std::move(processing_origins),
                        std::move(payload_contents), std::move(shared_info));
}

// static
absl::optional<AggregatableReportRequest>
AggregatableReportRequest::CreateForTesting(
    std::vector<url::Origin> processing_origins,
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info) {
  return CreateInternal(std::move(processing_origins),
                        std::move(payload_contents), std::move(shared_info));
}

// static
absl::optional<AggregatableReportRequest>
AggregatableReportRequest::CreateInternal(
    std::vector<url::Origin> processing_origins,
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info) {
  if (!AggregatableReport::IsNumberOfProcessingOriginsValid(
          processing_origins.size(), payload_contents.processing_type)) {
    return absl::nullopt;
  }

  if (!base::ranges::all_of(processing_origins,
                            network::IsOriginPotentiallyTrustworthy)) {
    return absl::nullopt;
  }

  if (payload_contents.value < 0) {
    return absl::nullopt;
  }

  if (!shared_info.report_id.is_valid()) {
    return absl::nullopt;
  }

  // Ensure the ordering of origins is deterministic. This is required for
  // AggregatableReport construction later.
  base::ranges::sort(processing_origins);

  return AggregatableReportRequest(std::move(processing_origins),
                                   std::move(payload_contents),
                                   std::move(shared_info));
}

AggregatableReportRequest::AggregatableReportRequest(
    std::vector<url::Origin> processing_origins,
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info)
    : processing_origins_(std::move(processing_origins)),
      payload_contents_(std::move(payload_contents)),
      shared_info_(std::move(shared_info)) {}

AggregatableReportRequest::AggregatableReportRequest(
    AggregatableReportRequest&& other) = default;

AggregatableReportRequest& AggregatableReportRequest::operator=(
    AggregatableReportRequest&& other) = default;

AggregatableReportRequest::~AggregatableReportRequest() = default;

AggregatableReport::AggregationServicePayload::AggregationServicePayload(
    std::vector<uint8_t> payload,
    std::string key_id,
    absl::optional<std::vector<uint8_t>> debug_cleartext_payload)
    : payload(std::move(payload)),
      key_id(std::move(key_id)),
      debug_cleartext_payload(std::move(debug_cleartext_payload)) {}

AggregatableReport::AggregationServicePayload::AggregationServicePayload(
    const AggregatableReport::AggregationServicePayload& other) = default;
AggregatableReport::AggregationServicePayload&
AggregatableReport::AggregationServicePayload::operator=(
    const AggregatableReport::AggregationServicePayload& other) = default;
AggregatableReport::AggregationServicePayload::AggregationServicePayload(
    AggregatableReport::AggregationServicePayload&& other) = default;
AggregatableReport::AggregationServicePayload&
AggregatableReport::AggregationServicePayload::operator=(
    AggregatableReport::AggregationServicePayload&& other) = default;
AggregatableReport::AggregationServicePayload::~AggregationServicePayload() =
    default;

AggregatableReport::AggregatableReport(
    std::vector<AggregationServicePayload> payloads,
    std::string shared_info)
    : payloads_(std::move(payloads)), shared_info_(std::move(shared_info)) {}

AggregatableReport::AggregatableReport(const AggregatableReport& other) =
    default;

AggregatableReport& AggregatableReport::operator=(
    const AggregatableReport& other) = default;

AggregatableReport::AggregatableReport(AggregatableReport&& other) = default;

AggregatableReport& AggregatableReport::operator=(AggregatableReport&& other) =
    default;

AggregatableReport::~AggregatableReport() = default;

constexpr size_t AggregatableReport::kBucketDomainBitLength;
constexpr size_t AggregatableReport::kValueDomainBitLength;
constexpr char AggregatableReport::kDomainSeparationPrefix[];

// static
bool AggregatableReport::Provider::g_disable_encryption_for_testing_tool_ =
    false;

// static
void AggregatableReport::Provider::SetDisableEncryptionForTestingTool(
    bool should_disable) {
  g_disable_encryption_for_testing_tool_ = should_disable;
}

AggregatableReport::Provider::~Provider() = default;

absl::optional<AggregatableReport>
AggregatableReport::Provider::CreateFromRequestAndPublicKeys(
    AggregatableReportRequest report_request,
    std::vector<PublicKey> public_keys) const {
  const size_t num_processing_origins = public_keys.size();
  DCHECK_EQ(num_processing_origins, report_request.processing_origins().size());

  // The origins must be sorted so we can ensure the ordering (and assignment of
  // DpfKey parties for two-party processing types) is deterministic.
  DCHECK(base::ranges::is_sorted(report_request.processing_origins()));

  std::vector<std::vector<uint8_t>> unencrypted_payloads;

  switch (report_request.payload_contents().processing_type) {
    case AggregationServicePayloadContents::ProcessingType::kTwoParty: {
      unencrypted_payloads = ConstructUnencryptedTwoPartyPayloads(
          report_request.payload_contents());
      break;
    }
    case AggregationServicePayloadContents::ProcessingType::kSingleServer: {
      unencrypted_payloads = ConstructUnencryptedSingleServerPayload(
          report_request.payload_contents());
      break;
    }
  }

  if (unencrypted_payloads.empty()) {
    return absl::nullopt;
  }

  std::vector<uint8_t> authenticated_info(
      kDomainSeparationPrefix,
      kDomainSeparationPrefix + sizeof(kDomainSeparationPrefix));

  std::string encoded_shared_info =
      report_request.shared_info_.SerializeAsJson();
  authenticated_info.insert(authenticated_info.end(),
                            encoded_shared_info.begin(),
                            encoded_shared_info.end());

  // To avoid unnecessary copies, we move the processing origins and shared info
  // from the `report_request`'s private members. Note that the request object
  // is destroyed at the end of this method.
  std::vector<AggregatableReport::AggregationServicePayload> encrypted_payloads;
  DCHECK_EQ(unencrypted_payloads.size(), num_processing_origins);
  for (size_t i = 0; i < num_processing_origins; ++i) {
    std::vector<uint8_t> encrypted_payload =
        g_disable_encryption_for_testing_tool_
            ? unencrypted_payloads[i]
            : EncryptWithHpke(
                  /*plaintext=*/unencrypted_payloads[i],
                  /*public_key=*/public_keys[i].key,
                  /*authenticated_info=*/authenticated_info);

    if (encrypted_payload.empty()) {
      return absl::nullopt;
    }

    absl::optional<std::vector<uint8_t>> debug_cleartext_payload;
    if (report_request.shared_info().debug_mode ==
        AggregatableReportSharedInfo::DebugMode::kEnabled) {
      debug_cleartext_payload = std::move(unencrypted_payloads[i]);
    }

    encrypted_payloads.emplace_back(std::move(encrypted_payload),
                                    std::move(public_keys[i]).id,
                                    std::move(debug_cleartext_payload));
  }

  return AggregatableReport(std::move(encrypted_payloads),
                            std::move(encoded_shared_info));
}

base::Value::DictStorage AggregatableReport::GetAsJson() const {
  base::Value::DictStorage value;

  value.emplace("shared_info", shared_info_);

  base::Value payloads_list_value(base::Value::Type::LIST);
  for (const AggregationServicePayload& payload : payloads_) {
    base::Value payload_dict_value(base::Value::Type::DICTIONARY);
    payload_dict_value.SetStringKey("payload",
                                    base::Base64Encode(payload.payload));
    payload_dict_value.SetStringKey("key_id", payload.key_id);
    if (payload.debug_cleartext_payload.has_value()) {
      payload_dict_value.SetStringKey(
          "debug_cleartext_payload",
          base::Base64Encode(payload.debug_cleartext_payload.value()));
    }

    payloads_list_value.Append(std::move(payload_dict_value));
  }

  value.emplace("aggregation_service_payloads", std::move(payloads_list_value));

  return value;
}

// static
bool AggregatableReport::IsNumberOfProcessingOriginsValid(
    size_t number,
    AggregationServicePayloadContents::ProcessingType processing_type) {
  switch (processing_type) {
    case AggregationServicePayloadContents::ProcessingType::kTwoParty:
      return number == 2u;
    case AggregationServicePayloadContents::ProcessingType::kSingleServer:
      return number == 1u;
  }
}

}  // namespace content
