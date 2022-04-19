// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/public_key.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
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

std::vector<GURL> GetDefaultProcessingUrls(
    AggregationServicePayloadContents::AggregationMode aggregation_mode) {
  switch (aggregation_mode) {
    case AggregationServicePayloadContents::AggregationMode::kTeeBased:
      return {
          GURL(kPrivacySandboxAggregationServiceTrustedServerUrlParam.Get())};
    case AggregationServicePayloadContents::AggregationMode::
        kExperimentalPoplar:
      // TODO(crbug.com/1295705): Update default processing urls.
      return {GURL("https://server1.example"), GURL("https://server2.example")};
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
  DCHECK_EQ(
      contents.aggregation_mode,
      AggregationServicePayloadContents::AggregationMode::kExperimentalPoplar);
  DCHECK_EQ(contents.contributions.size(), 1u);

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
          /*alpha=*/contents.contributions[0].bucket,
          /*beta=*/std::vector<absl::uint128>(
              AggregatableReport::kBucketDomainBitLength,
              contents.contributions[0].value));
  if (!status_or_dpf_keys.ok()) {
    return {};
  }

  std::vector<DpfKey> dpf_keys;
  dpf_keys.push_back(std::move(status_or_dpf_keys->first));
  dpf_keys.push_back(std::move(status_or_dpf_keys->second));

  return dpf_keys;
}

// Returns a vector with a serialized CBOR map for each processing url. See
// the AggregatableReport documentation for more detail on the expected format.
// Returns an empty vector in case of error.
std::vector<std::vector<uint8_t>>
ConstructUnencryptedExperimentalPoplarPayloads(
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
template <typename T>
std::vector<uint8_t> EncodeIntegerForPayload(T integer) {
  static_assert(sizeof(T) <= sizeof(absl::uint128),
                "sizeof(T) <= sizeof(absl::uint128)");
  static_assert(!std::numeric_limits<T>::is_signed,
                "!std::numeric_limits<T>::is_signed");
  static_assert(std::is_integral_v<T> || std::is_same_v<T, absl::uint128>,
                "std::is_integral_v<T> || std::is_same_v<T, absl::uint128>");
  std::vector<uint8_t> byte_string(sizeof(T));

  // Construct the vector in reverse to ensure network byte (big-endian) order.
  for (auto it = byte_string.rbegin(); it != byte_string.rend(); ++it) {
    *it = static_cast<uint8_t>(integer & 0xFF);
    integer >>= 8;
  }
  DCHECK_EQ(integer, 0u);
  return byte_string;
}

// Returns a vector with a serialized CBOR map. See the AggregatableReport
// documentation for more detail on the expected format. Returns an empty
// vector in case of error.
// Note that a vector is returned to match the `kExperimentalPoplar` case.
std::vector<std::vector<uint8_t>> ConstructUnencryptedTeeBasedPayload(
    const AggregationServicePayloadContents& payload_contents) {
  cbor::Value::MapValue value;
  value.emplace(kOperationKey, kHistogramValue);

  cbor::Value::ArrayValue data;
  for (AggregationServicePayloadContents::HistogramContribution contribution :
       payload_contents.contributions) {
    cbor::Value::MapValue data_map;
    data_map.emplace(
        "bucket", EncodeIntegerForPayload<absl::uint128>(contribution.bucket));
    data_map.emplace("value",
                     EncodeIntegerForPayload<uint32_t>(contribution.value));
    data.push_back(cbor::Value(std::move(data_map)));
  }
  value.emplace("data", std::move(data));

  absl::optional<std::vector<uint8_t>> unencrypted_payload =
      cbor::Writer::Write(cbor::Value(std::move(value)));

  if (!unencrypted_payload.has_value()) {
    return {};
  }

  return {std::move(unencrypted_payload.value())};
}

// Encrypts the `plaintext` with HPKE using the processing url's
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
    std::vector<AggregationServicePayloadContents::HistogramContribution>
        contributions,
    AggregationMode aggregation_mode)
    : operation(operation),
      contributions(std::move(contributions)),
      aggregation_mode(aggregation_mode) {}

AggregationServicePayloadContents::AggregationServicePayloadContents(
    const AggregationServicePayloadContents& other) = default;
AggregationServicePayloadContents& AggregationServicePayloadContents::operator=(
    const AggregationServicePayloadContents& other) = default;
AggregationServicePayloadContents::AggregationServicePayloadContents(
    AggregationServicePayloadContents&& other) = default;
AggregationServicePayloadContents& AggregationServicePayloadContents::operator=(
    AggregationServicePayloadContents&& other) = default;

AggregationServicePayloadContents::~AggregationServicePayloadContents() =
    default;

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
  base::Value::Dict value;

  value.Set("privacy_budget_key", privacy_budget_key);

  DCHECK(report_id.is_valid());
  value.Set("report_id", report_id.AsLowercaseString());

  value.Set("reporting_origin", reporting_origin.Serialize());

  // Encoded as the number of seconds since the Unix epoch, ignoring leap
  // seconds and rounded down.
  DCHECK(!scheduled_report_time.is_null());
  DCHECK(!scheduled_report_time.is_inf());
  value.Set("scheduled_report_time",
            base::NumberToString(scheduled_report_time.ToJavaTime() /
                                 base::Time::kMillisecondsPerSecond));

  // TODO(alexmt): Replace with a real version once a version string is decided.
  value.Set("version", "");

  // Only include the field if enabled.
  if (debug_mode == DebugMode::kEnabled) {
    value.Set("debug_mode", "enabled");
  }

  std::string serialized_value;
  bool succeeded = base::JSONWriter::Write(value, &serialized_value);
  DCHECK(succeeded);

  return serialized_value;
}

// static
absl::optional<AggregatableReportRequest> AggregatableReportRequest::Create(
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info) {
  std::vector<GURL> processing_urls =
      GetDefaultProcessingUrls(payload_contents.aggregation_mode);
  return CreateInternal(std::move(processing_urls), std::move(payload_contents),
                        std::move(shared_info));
}

// static
absl::optional<AggregatableReportRequest>
AggregatableReportRequest::CreateForTesting(
    std::vector<GURL> processing_urls,
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info) {
  return CreateInternal(std::move(processing_urls), std::move(payload_contents),
                        std::move(shared_info));
}

// static
absl::optional<AggregatableReportRequest>
AggregatableReportRequest::CreateInternal(
    std::vector<GURL> processing_urls,
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info) {
  if (!AggregatableReport::IsNumberOfProcessingUrlsValid(
          processing_urls.size(), payload_contents.aggregation_mode)) {
    return absl::nullopt;
  }

  if (!base::ranges::all_of(processing_urls,
                            network::IsUrlPotentiallyTrustworthy)) {
    return absl::nullopt;
  }

  if (!AggregatableReport::IsNumberOfHistogramContributionsValid(
          payload_contents.contributions.size(),
          payload_contents.aggregation_mode)) {
    return absl::nullopt;
  }

  if (base::ranges::any_of(
          payload_contents.contributions,
          [](const AggregationServicePayloadContents::HistogramContribution&
                 contribution) { return contribution.value < 0; })) {
    return absl::nullopt;
  }

  if (!shared_info.report_id.is_valid()) {
    return absl::nullopt;
  }

  if (!base::ranges::all_of(shared_info.privacy_budget_key,
                            &base::IsAsciiPrintable<char>)) {
    return absl::nullopt;
  }

  // Ensure the ordering of urls is deterministic. This is required for
  // AggregatableReport construction later.
  base::ranges::sort(processing_urls);

  return AggregatableReportRequest(std::move(processing_urls),
                                   std::move(payload_contents),
                                   std::move(shared_info));
}

AggregatableReportRequest::AggregatableReportRequest(
    std::vector<GURL> processing_urls,
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info)
    : processing_urls_(std::move(processing_urls)),
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
  const size_t num_processing_urls = public_keys.size();
  DCHECK_EQ(num_processing_urls, report_request.processing_urls().size());

  // The urls must be sorted so we can ensure the ordering (and assignment of
  // DpfKey parties for the `kExperimentalPoplar` aggregation mode) is
  // deterministic.
  DCHECK(base::ranges::is_sorted(report_request.processing_urls()));

  std::vector<std::vector<uint8_t>> unencrypted_payloads;

  switch (report_request.payload_contents().aggregation_mode) {
    case AggregationServicePayloadContents::AggregationMode::kTeeBased: {
      unencrypted_payloads = ConstructUnencryptedTeeBasedPayload(
          report_request.payload_contents());
      break;
    }
    case AggregationServicePayloadContents::AggregationMode::
        kExperimentalPoplar: {
      unencrypted_payloads = ConstructUnencryptedExperimentalPoplarPayloads(
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
      report_request.shared_info().SerializeAsJson();
  authenticated_info.insert(authenticated_info.end(),
                            encoded_shared_info.begin(),
                            encoded_shared_info.end());

  std::vector<AggregatableReport::AggregationServicePayload> encrypted_payloads;
  DCHECK_EQ(unencrypted_payloads.size(), num_processing_urls);
  for (size_t i = 0; i < num_processing_urls; ++i) {
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

base::Value::Dict AggregatableReport::GetAsJson() const {
  base::Value::Dict value;

  value.Set("shared_info", shared_info_);

  base::Value::List payloads_list_value;
  for (const AggregationServicePayload& payload : payloads_) {
    base::Value::Dict payload_dict_value;
    payload_dict_value.Set("payload", base::Base64Encode(payload.payload));
    payload_dict_value.Set("key_id", payload.key_id);
    if (payload.debug_cleartext_payload.has_value()) {
      payload_dict_value.Set(
          "debug_cleartext_payload",
          base::Base64Encode(payload.debug_cleartext_payload.value()));
    }

    payloads_list_value.Append(std::move(payload_dict_value));
  }

  value.Set("aggregation_service_payloads", std::move(payloads_list_value));

  return value;
}

// static
bool AggregatableReport::IsNumberOfProcessingUrlsValid(
    size_t number,
    AggregationServicePayloadContents::AggregationMode aggregation_mode) {
  switch (aggregation_mode) {
    case AggregationServicePayloadContents::AggregationMode::kTeeBased:
      return number == 1u;
    case AggregationServicePayloadContents::AggregationMode::
        kExperimentalPoplar:
      return number == 2u;
  }
}

// static
bool AggregatableReport::IsNumberOfHistogramContributionsValid(
    size_t number,
    AggregationServicePayloadContents::AggregationMode aggregation_mode) {
  // Note: APIs using the aggregation service may impose their own limits.
  switch (aggregation_mode) {
    case AggregationServicePayloadContents::AggregationMode::kTeeBased:
      return number >= 1u;
    case AggregationServicePayloadContents::AggregationMode::
        kExperimentalPoplar:
      return number == 1u;
  }
}

}  // namespace content
