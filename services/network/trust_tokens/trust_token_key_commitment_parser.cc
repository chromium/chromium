// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitment_parser.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"

namespace network {

const char kTrustTokenKeyCommitmentProtocolVersionField[] = "protocol_version";
const char kTrustTokenKeyCommitmentIDField[] = "id";
const char kTrustTokenKeyCommitmentBatchsizeField[] = "batchsize";
const char kTrustTokenKeyCommitmentExpiryField[] = "expiry";
const char kTrustTokenKeyCommitmentKeyField[] = "Y";
const char kTrustTokenKeyCommitmentRequestIssuanceLocallyOnField[] =
    "request_issuance_locally_on";
const char kTrustTokenLocalOperationOsAndroid[] = "android";
const char kTrustTokenKeyCommitmentUnavailableLocalOperationFallbackField[] =
    "unavailable_local_operation_fallback";
const char kTrustTokenLocalOperationFallbackWebIssuance[] = "web_issuance";
const char kTrustTokenLocalOperationFallbackReturnWithError[] =
    "return_with_error";

namespace {

// Parses a single key label. If |in| is the string representation of an integer
// in in the representable range of uint32_t, returns true. Otherwise, returns
// false.
bool ParseSingleKeyLabel(base::StringPiece in) {
  uint64_t key_label_in_uint64;
  if (!base::StringToUint64(in, &key_label_in_uint64))
    return false;
  if (!base::IsValueInRangeForNumericType<uint32_t>(key_label_in_uint64))
    return false;
  return true;
}

enum class ParseKeyResult {
  // Continue as if the key didn't exist.
  kIgnore,
  // Fail parsing totally.
  kFail,
  // Parsing the key succeeded.
  kSucceed
};

// Parses a single key, consisting of a body (the key material) and an expiry
// timestamp. Fails the parse if either field is missing or malformed. If the
// key has expired but is otherwise valid, ignores the key rather than failing
// the prase.
ParseKeyResult ParseSingleKeyExceptLabel(
    const base::Value& in,
    mojom::TrustTokenVerificationKey* out) {
  CHECK(in.is_dict());

  const std::string* expiry =
      in.FindStringKey(kTrustTokenKeyCommitmentExpiryField);
  const std::string* key_body =
      in.FindStringKey(kTrustTokenKeyCommitmentKeyField);
  if (!expiry || !key_body)
    return ParseKeyResult::kFail;

  uint64_t expiry_microseconds_since_unix_epoch;
  if (!base::StringToUint64(*expiry, &expiry_microseconds_since_unix_epoch))
    return ParseKeyResult::kFail;

  if (!base::Base64Decode(*key_body, &out->body))
    return ParseKeyResult::kFail;

  out->expiry =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMicroseconds(expiry_microseconds_since_unix_epoch);
  if (out->expiry <= base::Time::Now())
    return ParseKeyResult::kIgnore;

  return ParseKeyResult::kSucceed;
}

base::Optional<mojom::TrustTokenKeyCommitmentResult::Os> ParseOs(
    base::StringPiece os_string) {
  if (os_string == kTrustTokenLocalOperationOsAndroid)
    return mojom::TrustTokenKeyCommitmentResult::Os::kAndroid;
  return base::nullopt;
}

// Attempts to parse a string representation of a member of the
// UnavailableLocalOperationFallback enum, returning true on success and false
// on failure.
bool ParseUnavailableLocalOperationFallback(
    base::StringPiece fallback_string,
    mojom::TrustTokenKeyCommitmentResult::UnavailableLocalOperationFallback*
        fallback_out) {
  if (fallback_string == kTrustTokenLocalOperationFallbackWebIssuance) {
    *fallback_out = mojom::TrustTokenKeyCommitmentResult::
        UnavailableLocalOperationFallback::kWebIssuance;
    return true;
  }
  if (fallback_string == kTrustTokenLocalOperationFallbackReturnWithError) {
    *fallback_out = mojom::TrustTokenKeyCommitmentResult::
        UnavailableLocalOperationFallback::kReturnWithError;
    return true;
  }
  return false;
}

// Given a per-issuer key commitment dictionary, looks for the local Trust
// Tokens issuance-related fields request_issuance_locally_on and
// unavailable_local_operation_fallback.
//
// Returns true if both are absent, or if both are present and well-formed; in
// the latter case, updates |result| to with their parsed values. Otherwise,
// returns false.
bool ParseLocalOperationFieldsIfPresent(
    const base::Value& value,
    mojom::TrustTokenKeyCommitmentResult* result) {
  const base::Value* maybe_request_issuance_locally_on =
      value.FindKey(kTrustTokenKeyCommitmentRequestIssuanceLocallyOnField);

  // The local issuance field is optional...
  if (!maybe_request_issuance_locally_on)
    return true;

  // ...but needs to be the right type if it's provided.
  if (!maybe_request_issuance_locally_on->is_list())
    return false;

  for (const base::Value& maybe_os_value :
       maybe_request_issuance_locally_on->GetList()) {
    if (!maybe_os_value.is_string())
      return false;
    base::Optional<mojom::TrustTokenKeyCommitmentResult::Os> maybe_os =
        ParseOs(maybe_os_value.GetString());
    if (!maybe_os)
      return false;
    result->request_issuance_locally_on.push_back(*maybe_os);
  }

  // Deduplicate the OS values:
  auto& oses = result->request_issuance_locally_on;
  base::ranges::sort(oses);
  auto to_remove = base::ranges::unique(oses);
  oses.erase(to_remove, oses.end());

  const std::string* maybe_fallback = value.FindStringKey(
      kTrustTokenKeyCommitmentUnavailableLocalOperationFallbackField);
  if (!maybe_fallback ||
      !ParseUnavailableLocalOperationFallback(
          *maybe_fallback, &result->unavailable_local_operation_fallback)) {
    return false;
  }

  return true;
}

mojom::TrustTokenKeyCommitmentResultPtr ParseSingleIssuer(
    const base::Value& value) {
  if (!value.is_dict())
    return nullptr;

  auto result = mojom::TrustTokenKeyCommitmentResult::New();

  // Confirm that the protocol_version field is present.
  const std::string* maybe_version =
      value.FindStringKey(kTrustTokenKeyCommitmentProtocolVersionField);
  if (!maybe_version)
    return nullptr;
  if (*maybe_version == "TrustTokenV2PMB") {
    result->protocol_version =
        mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  } else if (*maybe_version == "TrustTokenV2VOPRF") {
    result->protocol_version =
        mojom::TrustTokenProtocolVersion::kTrustTokenV2Voprf;
  } else {
    return nullptr;
  }

  // Confirm that the id field is present and type-safe.
  base::Optional<int> maybe_id =
      value.FindIntKey(kTrustTokenKeyCommitmentIDField);
  if (!maybe_id || *maybe_id <= 0)
    return nullptr;
  result->id = *maybe_id;

  // Confirm that the batchsize field is present and type-safe.
  base::Optional<int> maybe_batch_size =
      value.FindIntKey(kTrustTokenKeyCommitmentBatchsizeField);
  if (!maybe_batch_size || *maybe_batch_size <= 0)
    return nullptr;
  result->batch_size = *maybe_batch_size;

  if (!ParseLocalOperationFieldsIfPresent(value, result.get()))
    return nullptr;

  // Parse the key commitments in the result (these are exactly the
  // key-value pairs in the dictionary with dictionary-typed values).
  for (const auto& kv : value.DictItems()) {
    const base::Value& item = kv.second;
    if (!item.is_dict())
      continue;

    auto key = mojom::TrustTokenVerificationKey::New();

    if (!ParseSingleKeyLabel(kv.first))
      return nullptr;

    switch (ParseSingleKeyExceptLabel(item, key.get())) {
      case ParseKeyResult::kFail:
        return nullptr;
      case ParseKeyResult::kIgnore:
        continue;
      case ParseKeyResult::kSucceed:
        result->keys.push_back(std::move(key));
    }
  }

  return result;
}

// Entry is a convenience struct used as an intermediate representation when
// parsing multiple issuers. In addition to a parsed canonicalized issuer, it
// preserves the raw JSON string key (the second entry) in order
// deterministically to deduplicate entries with keys canonicalizing to the same
// issuer.
using Entry = std::tuple<SuitableTrustTokenOrigin,  // canonicalized issuer
                         std::string,               // raw key from the JSON
                         mojom::TrustTokenKeyCommitmentResultPtr>;
SuitableTrustTokenOrigin& canonicalized_issuer(Entry& e) {
  return std::get<0>(e);
}
mojom::TrustTokenKeyCommitmentResultPtr& commitment(Entry& e) {
  return std::get<2>(e);
}

}  // namespace

mojom::TrustTokenKeyCommitmentResultPtr TrustTokenKeyCommitmentParser::Parse(
    base::StringPiece response_body) {
  base::Optional<base::Value> maybe_value =
      base::JSONReader::Read(response_body);
  if (!maybe_value)
    return nullptr;

  return ParseSingleIssuer(std::move(*maybe_value));
}

std::unique_ptr<base::flat_map<SuitableTrustTokenOrigin,
                               mojom::TrustTokenKeyCommitmentResultPtr>>
TrustTokenKeyCommitmentParser::ParseMultipleIssuers(
    base::StringPiece response_body) {
  base::Optional<base::Value> maybe_value =
      base::JSONReader::Read(response_body);
  if (!maybe_value)
    return nullptr;

  if (!maybe_value->is_dict())
    return nullptr;

  // The configuration might contain conflicting lists of keys for issuers with
  // the same canonicalized URLs but different string representations provided
  // by the server. In order to handle these deterministically, first transfer
  // the entries to intermediate storage while maintaining the initial JSON
  // keys; then deduplicate based on identical entries' JSON keys' lexicographic
  // value.

  std::vector<Entry> parsed_entries;

  for (const auto& kv : maybe_value->DictItems()) {
    const std::string& raw_key_from_json = kv.first;
    base::Optional<SuitableTrustTokenOrigin> maybe_issuer =
        SuitableTrustTokenOrigin::Create(GURL(raw_key_from_json));

    if (!maybe_issuer)
      continue;

    mojom::TrustTokenKeyCommitmentResultPtr commitment_result =
        ParseSingleIssuer(kv.second);

    if (!commitment_result)
      continue;

    parsed_entries.emplace_back(Entry(std::move(*maybe_issuer),
                                      raw_key_from_json,
                                      std::move(commitment_result)));
  }

  // Deterministically deduplicate entries corresponding to the same issuer,
  // with the largest JSON key lexicographically winning.
  std::sort(parsed_entries.begin(), parsed_entries.end(), std::greater<>());
  parsed_entries.erase(std::unique(parsed_entries.begin(), parsed_entries.end(),
                                   [](Entry& lhs, Entry& rhs) -> bool {
                                     return canonicalized_issuer(lhs) ==
                                            canonicalized_issuer(rhs);
                                   }),
                       parsed_entries.end());

  // Finally, discard the raw JSON strings and construct a map to return.
  std::vector<std::pair<SuitableTrustTokenOrigin,
                        mojom::TrustTokenKeyCommitmentResultPtr>>
      map_storage;
  map_storage.reserve(parsed_entries.size());
  for (Entry& e : parsed_entries) {
    map_storage.emplace_back(std::move(canonicalized_issuer(e)),
                             std::move(commitment(e)));
  }

  // Please don't remove this VLOG without first checking with
  // trust_tokens/OWNERS to see if it's still being used for manual
  // testing.
  VLOG(1) << "Successfully parsed " << map_storage.size()
          << " issuers' Trust Tokens key commitments.";

  return std::make_unique<base::flat_map<
      SuitableTrustTokenOrigin, mojom::TrustTokenKeyCommitmentResultPtr>>(
      std::move(map_storage));
}

}  // namespace network
