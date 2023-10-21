// Copyright 2021 The Chromium Authors
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
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/aggregation_service/features.h"
#include "components/aggregation_service/parsing_utils.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/proto/aggregatable_report.pb.h"
#include "content/browser/aggregation_service/public_key.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
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
    blink::mojom::AggregationServiceMode aggregation_mode,
    const absl::optional<url::Origin>& aggregation_coordinator_origin) {
  switch (aggregation_mode) {
    case blink::mojom::AggregationServiceMode::kTeeBased:
      if (base::FeatureList::IsEnabled(
              aggregation_service::kAggregationServiceMultipleCloudProviders)) {
        if (!aggregation_coordinator_origin.has_value()) {
          return {GetAggregationServiceProcessingUrl(
              ::aggregation_service::GetDefaultAggregationCoordinatorOrigin())};
        }
        if (!::aggregation_service::IsAggregationCoordinatorOriginAllowed(
                *aggregation_coordinator_origin)) {
          return {};
        }
        return {GetAggregationServiceProcessingUrl(
            *aggregation_coordinator_origin)};
      } else {
        return {GURL(
            kPrivacySandboxAggregationServiceTrustedServerUrlAwsParam.Get())};
      }
    case blink::mojom::AggregationServiceMode::kExperimentalPoplar:
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
  DCHECK_EQ(contents.aggregation_mode,
            blink::mojom::AggregationServiceMode::kExperimentalPoplar);
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

void AppendEncodedContributionToCborArray(
    cbor::Value::ArrayValue& array,
    const blink::mojom::AggregatableReportHistogramContribution& contribution) {
  cbor::Value::MapValue map;
  map.emplace("bucket",
              EncodeIntegerForPayload<absl::uint128>(contribution.bucket));
  map.emplace("value", EncodeIntegerForPayload<uint32_t>(contribution.value));
  array.emplace_back(std::move(map));
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
  base::ranges::for_each(
      payload_contents.contributions,
      [&data](const blink::mojom::AggregatableReportHistogramContribution&
                  contribution) {
        AppendEncodedContributionToCborArray(data, contribution);
      });

  int number_of_null_contributions_to_add = 0;
  if (base::FeatureList::IsEnabled(
          kPrivacySandboxAggregationServiceReportPadding)) {
    number_of_null_contributions_to_add =
        payload_contents.max_contributions_allowed -
        payload_contents.contributions.size();
  } else if (payload_contents.contributions.empty()) {
    number_of_null_contributions_to_add = 1;
  }
  CHECK_GE(number_of_null_contributions_to_add, 0);

  for (int i = 0; i < number_of_null_contributions_to_add; ++i) {
    AppendEncodedContributionToCborArray(
        data, blink::mojom::AggregatableReportHistogramContribution(
                  /*bucket=*/0, /*value=*/0));
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
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> public_key,
    base::span<const uint8_t> authenticated_info) {
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

absl::optional<AggregationServicePayloadContents>
ConvertPayloadContentsFromProto(
    const proto::AggregationServicePayloadContents& proto) {
  if (proto.operation() !=
      proto::AggregationServicePayloadContents_Operation_HISTOGRAM) {
    return absl::nullopt;
  }
  AggregationServicePayloadContents::Operation operation(
      AggregationServicePayloadContents::Operation::kHistogram);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;
  for (const proto::AggregatableReportHistogramContribution&
           contribution_proto : proto.contributions()) {
    contributions.emplace_back(
        /*bucket=*/absl::MakeUint128(contribution_proto.bucket_high(),
                                     contribution_proto.bucket_low()),
        /*value=*/contribution_proto.value());
  }

  blink::mojom::AggregationServiceMode aggregation_mode =
      blink::mojom::AggregationServiceMode::kTeeBased;
  switch (proto.aggregation_mode()) {
    case proto::AggregationServiceMode::TEE_BASED:
      break;
    case proto::AggregationServiceMode::EXPERIMENTAL_POPLAR:
      aggregation_mode =
          blink::mojom::AggregationServiceMode::kExperimentalPoplar;
      break;
    default:
      return absl::nullopt;
  }

  absl::optional<url::Origin> aggregation_coordinator_origin;
  if (proto.has_aggregation_coordinator_origin()) {
    aggregation_coordinator_origin =
        url::Origin::Create(GURL(proto.aggregation_coordinator_origin()));
  }

  int max_contributions_allowed = proto.max_contributions_allowed();
  if (max_contributions_allowed < 0) {
    return absl::nullopt;
  } else if (max_contributions_allowed == 0) {
    // Don't pad reports stored before padding was implemented.
    max_contributions_allowed = contributions.size();
  }

  // Report storage doesn't support multiple aggregation coordinators.
  return AggregationServicePayloadContents(
      operation, std::move(contributions), aggregation_mode,
      std::move(aggregation_coordinator_origin), max_contributions_allowed);
}

absl::optional<AggregatableReportSharedInfo> ConvertSharedInfoFromProto(
    const proto::AggregatableReportSharedInfo& proto) {
  base::Time scheduled_report_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(proto.scheduled_report_time()));
  base::Uuid report_id = base::Uuid::ParseLowercase(proto.report_id());
  url::Origin reporting_origin =
      url::Origin::Create(GURL(proto.reporting_origin()));

  AggregatableReportSharedInfo::DebugMode debug_mode =
      AggregatableReportSharedInfo::DebugMode::kDisabled;
  switch (proto.debug_mode()) {
    case proto::AggregatableReportSharedInfo_DebugMode_DISABLED:
      break;
    case proto::AggregatableReportSharedInfo_DebugMode_ENABLED:
      debug_mode = AggregatableReportSharedInfo::DebugMode::kEnabled;
      break;
    default:
      return absl::nullopt;
  }

  std::string api_version = proto.api_version();
  std::string api_identifier = proto.api_identifier();

  return AggregatableReportSharedInfo(
      scheduled_report_time, std::move(report_id), std::move(reporting_origin),
      debug_mode,
      // TODO(alexmt): Persist additional_fields when it becomes necessary.
      /*additional_fields=*/base::Value::Dict(),
      // TODO(crbug.com/1340296): Add mechanism to upgrade stored requests from
      // older to newer versions.
      std::move(api_version), std::move(api_identifier));
}

absl::optional<AggregatableReportRequest> ConvertReportRequestFromProto(
    proto::AggregatableReportRequest request_proto) {
  absl::optional<AggregationServicePayloadContents> payload_contents(
      ConvertPayloadContentsFromProto(request_proto.payload_contents()));
  if (!payload_contents.has_value()) {
    return absl::nullopt;
  }

  absl::optional<AggregatableReportSharedInfo> shared_info(
      ConvertSharedInfoFromProto(request_proto.shared_info()));
  if (!shared_info.has_value()) {
    return absl::nullopt;
  }

  absl::optional<uint64_t> debug_key;
  if (request_proto.has_debug_key()) {
    debug_key = request_proto.debug_key();
  }

  base::flat_map<std::string, std::string> additional_fields;
  for (auto& elem : request_proto.additional_fields()) {
    additional_fields.emplace(std::move(elem));
  }

  return AggregatableReportRequest::Create(
      std::move(payload_contents.value()), std::move(shared_info.value()),
      std::move(*request_proto.mutable_reporting_path()), debug_key,
      std::move(additional_fields), request_proto.failed_send_attempts());
}

void ConvertPayloadContentsToProto(
    const AggregationServicePayloadContents& payload_contents,
    proto::AggregationServicePayloadContents* out) {
  switch (payload_contents.operation) {
    case AggregationServicePayloadContents::Operation::kHistogram:
      out->set_operation(
          proto::AggregationServicePayloadContents_Operation_HISTOGRAM);
  }

  for (const blink::mojom::AggregatableReportHistogramContribution&
           contribution : payload_contents.contributions) {
    proto::AggregatableReportHistogramContribution* contribution_proto =
        out->add_contributions();
    contribution_proto->set_bucket_high(
        absl::Uint128High64(contribution.bucket));
    contribution_proto->set_bucket_low(absl::Uint128Low64(contribution.bucket));
    contribution_proto->set_value(contribution.value);
  }

  switch (payload_contents.aggregation_mode) {
    case blink::mojom::AggregationServiceMode::kTeeBased:
      out->set_aggregation_mode(proto::AggregationServiceMode::TEE_BASED);
      break;
    case blink::mojom::AggregationServiceMode::kExperimentalPoplar:
      out->set_aggregation_mode(
          proto::AggregationServiceMode::EXPERIMENTAL_POPLAR);
      break;
  }

  if (base::FeatureList::IsEnabled(
          aggregation_service::kAggregationServiceMultipleCloudProviders) &&
      payload_contents.aggregation_coordinator_origin.has_value()) {
    out->set_aggregation_coordinator_origin(
        payload_contents.aggregation_coordinator_origin->Serialize());
  }

  out->set_max_contributions_allowed(
      payload_contents.max_contributions_allowed);
}

void ConvertSharedInfoToProto(const AggregatableReportSharedInfo& shared_info,
                              proto::AggregatableReportSharedInfo* out) {
  out->set_scheduled_report_time(
      shared_info.scheduled_report_time.ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  out->set_report_id(shared_info.report_id.AsLowercaseString());
  out->set_reporting_origin(shared_info.reporting_origin.Serialize());

  switch (shared_info.debug_mode) {
    case AggregatableReportSharedInfo::DebugMode::kDisabled:
      out->set_debug_mode(
          proto::AggregatableReportSharedInfo_DebugMode_DISABLED);
      break;
    case AggregatableReportSharedInfo::DebugMode::kEnabled:
      out->set_debug_mode(
          proto::AggregatableReportSharedInfo_DebugMode_ENABLED);
      break;
  }

  DCHECK(shared_info.additional_fields.empty());

  out->set_api_version(shared_info.api_version);
  out->set_api_identifier(shared_info.api_identifier);
}

proto::AggregatableReportRequest ConvertReportRequestToProto(
    const AggregatableReportRequest& request) {
  proto::AggregatableReportRequest request_proto;
  ConvertPayloadContentsToProto(
      request.payload_contents(),
      /*out=*/request_proto.mutable_payload_contents());
  ConvertSharedInfoToProto(request.shared_info(),
                           /*out=*/request_proto.mutable_shared_info());
  *request_proto.mutable_reporting_path() = request.reporting_path();
  if (request.debug_key().has_value()) {
    request_proto.set_debug_key(request.debug_key().value());
  }
  request_proto.set_failed_send_attempts(request.failed_send_attempts());

  for (auto& elem : request.additional_fields()) {
    (*request_proto.mutable_additional_fields())[elem.first] = elem.second;
  }

  return request_proto;
}

void MaybeVerifyPayloadLength(size_t max_contributions_allowed,
                              size_t payload_length) {
  // TODO(alexmt): Replace with a more general method to ensure that the payload
  // length is deterministic.
  // Note that the 747 byte expectation derives from the following:
  // 27 (baseline size with no contributions) + 20 * 36 (size per contribution)
  if (max_contributions_allowed == 20 && payload_length != 747) {
    base::debug::DumpWithoutCrashing();
  }
}

}  // namespace

GURL GetAggregationServiceProcessingUrl(const url::Origin& origin) {
  GURL::Replacements replacements;
  static constexpr char kEndpointPath[] =
      ".well-known/aggregation-service/v1/public-keys";
  replacements.SetPathStr(kEndpointPath);
  return origin.GetURL().ReplaceComponents(replacements);
}

AggregationServicePayloadContents::AggregationServicePayloadContents(
    Operation operation,
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions,
    blink::mojom::AggregationServiceMode aggregation_mode,
    absl::optional<url::Origin> aggregation_coordinator_origin,
    int max_contributions_allowed)
    : operation(operation),
      contributions(std::move(contributions)),
      aggregation_mode(aggregation_mode),
      aggregation_coordinator_origin(std::move(aggregation_coordinator_origin)),
      max_contributions_allowed(max_contributions_allowed) {}

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
    base::Uuid report_id,
    url::Origin reporting_origin,
    DebugMode debug_mode,
    base::Value::Dict additional_fields,
    std::string api_version,
    std::string api_identifier)
    : scheduled_report_time(scheduled_report_time),
      report_id(std::move(report_id)),
      reporting_origin(std::move(reporting_origin)),
      debug_mode(debug_mode),
      additional_fields(std::move(additional_fields)),
      api_version(std::move(api_version)),
      api_identifier(std::move(api_identifier)) {}

AggregatableReportSharedInfo::AggregatableReportSharedInfo(
    AggregatableReportSharedInfo&& other) = default;
AggregatableReportSharedInfo& AggregatableReportSharedInfo::operator=(
    AggregatableReportSharedInfo&& other) = default;
AggregatableReportSharedInfo::~AggregatableReportSharedInfo() = default;

AggregatableReportSharedInfo AggregatableReportSharedInfo::Clone() const {
  return AggregatableReportSharedInfo(
      scheduled_report_time, report_id, reporting_origin, debug_mode,
      additional_fields.Clone(), api_version, api_identifier);
}

std::string AggregatableReportSharedInfo::SerializeAsJson() const {
  base::Value::Dict value;

  DCHECK(report_id.is_valid());
  value.Set("report_id", report_id.AsLowercaseString());

  value.Set("reporting_origin", reporting_origin.Serialize());

  // Encoded as the number of seconds since the Unix epoch, ignoring leap
  // seconds and rounded down.
  DCHECK(!scheduled_report_time.is_null());
  DCHECK(!scheduled_report_time.is_inf());
  value.Set("scheduled_report_time",
            base::NumberToString(
                scheduled_report_time.InMillisecondsSinceUnixEpoch() /
                base::Time::kMillisecondsPerSecond));

  value.Set("version", api_version);

  value.Set("api", api_identifier);

  // Only include the field if enabled.
  if (debug_mode == DebugMode::kEnabled) {
    value.Set("debug_mode", "enabled");
  }

  DCHECK(base::ranges::none_of(additional_fields, [&value](const auto& e) {
    return value.contains(e.first);
  })) << "Additional fields in shared_info cannot duplicate existing fields";

  value.Merge(additional_fields.Clone());

  std::string serialized_value;
  bool succeeded = base::JSONWriter::Write(value, &serialized_value);
  DCHECK(succeeded);

  return serialized_value;
}

// static
absl::optional<AggregatableReportRequest> AggregatableReportRequest::Create(
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info,
    std::string reporting_path,
    absl::optional<uint64_t> debug_key,
    base::flat_map<std::string, std::string> additional_fields,
    int failed_send_attempts) {
  std::vector<GURL> processing_urls =
      GetDefaultProcessingUrls(payload_contents.aggregation_mode,
                               payload_contents.aggregation_coordinator_origin);
  return CreateInternal(std::move(processing_urls), std::move(payload_contents),
                        std::move(shared_info), std::move(reporting_path),
                        debug_key, std::move(additional_fields),
                        failed_send_attempts);
}

// static
absl::optional<AggregatableReportRequest>
AggregatableReportRequest::CreateForTesting(
    std::vector<GURL> processing_urls,
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info,
    std::string reporting_path,
    absl::optional<uint64_t> debug_key,
    base::flat_map<std::string, std::string> additional_fields,
    int failed_send_attempts) {
  return CreateInternal(std::move(processing_urls), std::move(payload_contents),
                        std::move(shared_info), std::move(reporting_path),
                        debug_key, std::move(additional_fields),
                        failed_send_attempts);
}

// static
absl::optional<AggregatableReportRequest>
AggregatableReportRequest::CreateInternal(
    std::vector<GURL> processing_urls,
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info,
    std::string reporting_path,
    absl::optional<uint64_t> debug_key,
    base::flat_map<std::string, std::string> additional_fields,
    int failed_send_attempts) {
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
          [](const blink::mojom::AggregatableReportHistogramContribution&
                 contribution) { return contribution.value < 0; })) {
    return absl::nullopt;
  }

  if (!shared_info.report_id.is_valid()) {
    return absl::nullopt;
  }

  if (debug_key.has_value() &&
      shared_info.debug_mode ==
          AggregatableReportSharedInfo::DebugMode::kDisabled) {
    return absl::nullopt;
  }

  if (failed_send_attempts < 0) {
    return absl::nullopt;
  }

  if (payload_contents.max_contributions_allowed <
      static_cast<int>(payload_contents.contributions.size())) {
    return absl::nullopt;
  }

  // Ensure the ordering of urls is deterministic. This is required for
  // AggregatableReport construction later.
  base::ranges::sort(processing_urls);

  return AggregatableReportRequest(
      std::move(processing_urls), std::move(payload_contents),
      std::move(shared_info), std::move(reporting_path), debug_key,
      std::move(additional_fields), failed_send_attempts);
}

AggregatableReportRequest::AggregatableReportRequest(
    std::vector<GURL> processing_urls,
    AggregationServicePayloadContents payload_contents,
    AggregatableReportSharedInfo shared_info,
    std::string reporting_path,
    absl::optional<uint64_t> debug_key,
    base::flat_map<std::string, std::string> additional_fields,
    int failed_send_attempts)
    : processing_urls_(std::move(processing_urls)),
      payload_contents_(std::move(payload_contents)),
      shared_info_(std::move(shared_info)),
      reporting_path_(std::move(reporting_path)),
      debug_key_(debug_key),
      additional_fields_(std::move(additional_fields)),
      failed_send_attempts_(failed_send_attempts) {}

AggregatableReportRequest::AggregatableReportRequest(
    AggregatableReportRequest&& other) = default;

AggregatableReportRequest& AggregatableReportRequest::operator=(
    AggregatableReportRequest&& other) = default;

AggregatableReportRequest::~AggregatableReportRequest() = default;

GURL AggregatableReportRequest::GetReportingUrl() const {
  if (reporting_path_.empty()) {
    return GURL();
  }
  return shared_info().reporting_origin.GetURL().Resolve(reporting_path_);
}

absl::optional<AggregatableReportRequest>
AggregatableReportRequest::Deserialize(
    base::span<const uint8_t> serialized_proto) {
  proto::AggregatableReportRequest request_proto;
  if (!request_proto.ParseFromArray(serialized_proto.data(),
                                    serialized_proto.size())) {
    return absl::nullopt;
  }

  return ConvertReportRequestFromProto(std::move(request_proto));
}

std::vector<uint8_t> AggregatableReportRequest::Serialize() {
  proto::AggregatableReportRequest request_proto =
      ConvertReportRequestToProto(*this);

  size_t size = request_proto.ByteSizeLong();
  std::vector<uint8_t> serialized_proto(size);
  if (!request_proto.SerializeToArray(serialized_proto.data(), size)) {
    return {};
  }

  return serialized_proto;
}

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
    std::string shared_info,
    absl::optional<uint64_t> debug_key,
    base::flat_map<std::string, std::string> additional_fields,
    absl::optional<url::Origin> aggregation_coordinator_origin)
    : payloads_(std::move(payloads)),
      shared_info_(std::move(shared_info)),
      debug_key_(debug_key),
      additional_fields_(std::move(additional_fields)),
      aggregation_coordinator_origin_(
          std::move(aggregation_coordinator_origin)) {}

AggregatableReport::AggregatableReport(const AggregatableReport& other) =
    default;

AggregatableReport& AggregatableReport::operator=(
    const AggregatableReport& other) = default;

AggregatableReport::AggregatableReport(AggregatableReport&& other) = default;

AggregatableReport& AggregatableReport::operator=(AggregatableReport&& other) =
    default;

AggregatableReport::~AggregatableReport() = default;

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
    const AggregatableReportRequest& report_request,
    std::vector<PublicKey> public_keys) const {
  const size_t num_processing_urls = public_keys.size();
  DCHECK_EQ(num_processing_urls, report_request.processing_urls().size());

  // The urls must be sorted so we can ensure the ordering (and assignment of
  // DpfKey parties for the `kExperimentalPoplar` aggregation mode) is
  // deterministic.
  DCHECK(base::ranges::is_sorted(report_request.processing_urls()));

  std::vector<std::vector<uint8_t>> unencrypted_payloads;

  switch (report_request.payload_contents().aggregation_mode) {
    case blink::mojom::AggregationServiceMode::kTeeBased: {
      unencrypted_payloads = ConstructUnencryptedTeeBasedPayload(
          report_request.payload_contents());

      if (base::FeatureList::IsEnabled(
              kPrivacySandboxAggregationServiceReportPadding)) {
        MaybeVerifyPayloadLength(
            report_request.payload_contents().max_contributions_allowed,
            /*payload_length=*/unencrypted_payloads[0].size());
      }
      break;
    }
    case blink::mojom::AggregationServiceMode::kExperimentalPoplar: {
      unencrypted_payloads = ConstructUnencryptedExperimentalPoplarPayloads(
          report_request.payload_contents());
      break;
    }
  }

  if (unencrypted_payloads.empty()) {
    return absl::nullopt;
  }

  std::string encoded_shared_info =
      report_request.shared_info().SerializeAsJson();

  std::string authenticated_info_str =
      base::StrCat({kDomainSeparationPrefix, encoded_shared_info});
  base::span<const uint8_t> authenticated_info =
      base::as_bytes(base::make_span(authenticated_info_str));

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

  return AggregatableReport(
      std::move(encrypted_payloads), std::move(encoded_shared_info),
      report_request.debug_key(), report_request.additional_fields(),
      report_request.payload_contents().aggregation_coordinator_origin);
}

base::Value::Dict AggregatableReport::GetAsJson() const {
  base::Value::Dict value;

  value.Set("shared_info", shared_info_);

  // When invoked for reports being shown in the WebUI, `payloads_` may be empty
  // prior to assembly or if assembly failed.
  if (!payloads_.empty()) {
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
  }

  if (debug_key_.has_value()) {
    value.Set("debug_key", base::NumberToString(debug_key_.value()));
  }

  if (base::FeatureList::IsEnabled(
          aggregation_service::kAggregationServiceMultipleCloudProviders)) {
    value.Set(
        "aggregation_coordinator_origin",
        aggregation_coordinator_origin_
            .value_or(
                ::aggregation_service::GetDefaultAggregationCoordinatorOrigin())
            .Serialize());
  }

  for (const auto& item : additional_fields_) {
    CHECK(!value.contains(item.first))
        << "Additional field duplicates existing field: " << item.first;
    value.Set(item.first, item.second);
  }

  return value;
}

// static
bool AggregatableReport::IsNumberOfProcessingUrlsValid(
    size_t number,
    blink::mojom::AggregationServiceMode aggregation_mode) {
  switch (aggregation_mode) {
    case blink::mojom::AggregationServiceMode::kTeeBased:
      return number == 1u;
    case blink::mojom::AggregationServiceMode::kExperimentalPoplar:
      return number == 2u;
  }
}

// static
bool AggregatableReport::IsNumberOfHistogramContributionsValid(
    size_t number,
    blink::mojom::AggregationServiceMode aggregation_mode) {
  // Note: APIs using the aggregation service may impose their own limits.
  switch (aggregation_mode) {
    case blink::mojom::AggregationServiceMode::kTeeBased:
      return true;
    case blink::mojom::AggregationServiceMode::kExperimentalPoplar:
      return number == 1u;
  }
}

}  // namespace content
