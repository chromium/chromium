// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitment_parser.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/types.h"

namespace network {

const char kTrustTokenKeyCommitmentProtocolVersionField[] = "protocol_version";
const char kTrustTokenKeyCommitmentIDField[] = "id";
const char kTrustTokenKeyCommitmentBatchsizeField[] = "batchsize";
const char kTrustTokenKeyCommitmentKeysField[] = "keys";
const char kTrustTokenKeyCommitmentExpiryField[] = "expiry";
const char kTrustTokenKeyCommitmentKeyField[] = "Y";

namespace {

// Parses a single key label. If |in| is the string representation of an integer
// in in the representable range of uint32_t, returns true. Otherwise, returns
// false.
bool ParseSingleKeyLabel(std::string_view in) {
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
    const base::Value::Dict& in,
    mojom::TrustTokenVerificationKey* out) {
  const std::string* expiry =
      in.FindString(kTrustTokenKeyCommitmentExpiryField);
  const std::string* key_body = in.FindString(kTrustTokenKeyCommitmentKeyField);
  if (!expiry || !key_body)
    return ParseKeyResult::kFail;

  uint64_t expiry_microseconds_since_unix_epoch;
  if (!base::StringToUint64(*expiry, &expiry_microseconds_since_unix_epoch))
    return ParseKeyResult::kFail;

  if (!base::Base64Decode(*key_body, &out->body))
    return ParseKeyResult::kFail;

  out->expiry = base::Time::UnixEpoch() +
                base::Microseconds(expiry_microseconds_since_unix_epoch);
  if (out->expiry <= base::Time::Now())
    return ParseKeyResult::kIgnore;

  return ParseKeyResult::kSucceed;
}

mojom::TrustTokenKeyCommitmentResultPtr ParseSingleIssuer(
    const base::Value& commitments_by_version) {
  if (!commitments_by_version.is_dict())
    return nullptr;
  const base::Value::Dict& commitments_dict = commitments_by_version.GetDict();

  auto result = mojom::TrustTokenKeyCommitmentResult::New();

  const base::Value::Dict* dict = nullptr;
  // Confirm that the protocol_version field is present. If the server supports
  // multiple versions, we prefer the VOPRF version, since it's more efficient
  // (and we're free to choose which version to use).
  for (auto version :
       {mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Voprf,
        mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Pmb,
        mojom::TrustTokenProtocolVersion::kTrustTokenV3Voprf,
        mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb}) {
    std::string version_label = internal::ProtocolVersionToString(version);
    if (commitments_dict.contains(version_label)) {
      dict = commitments_dict.FindDict(version_label);
      if (!dict)
        return nullptr;
      const std::string* maybe_version =
          dict->FindString(kTrustTokenKeyCommitmentProtocolVersionField);
      if (!maybe_version || *maybe_version != version_label)
        return nullptr;
      result->protocol_version = version;
      break;
    }
  }
  if (!dict)
    return nullptr;

  // Confirm that the id field is present and type-safe.
  std::optional<int> maybe_id = dict->FindInt(kTrustTokenKeyCommitmentIDField);
  if (!maybe_id || *maybe_id <= 0)
    return nullptr;
  result->id = *maybe_id;

  // Confirm that the batchsize field is present and type-safe.
  std::optional<int> maybe_batch_size =
      dict->FindInt(kTrustTokenKeyCommitmentBatchsizeField);
  if (!maybe_batch_size || *maybe_batch_size <= 0)
    return nullptr;
  result->batch_size = *maybe_batch_size;

  // Parse the key commitments in the result if available.
  const base::Value* maybe_keys = dict->Find(kTrustTokenKeyCommitmentKeysField);
  if (!maybe_keys)
    return result;
  if (!maybe_keys->is_dict())
    return nullptr;
  for (auto kv : maybe_keys->GetDict()) {
    const base::Value& item = kv.second;
    if (!item.is_dict())
      continue;

    auto key = mojom::TrustTokenVerificationKey::New();

    if (!ParseSingleKeyLabel(kv.first))
      return nullptr;

    switch (ParseSingleKeyExceptLabel(item.GetDict(), key.get())) {
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
    std::string_view response_body) {
  std::optional<base::Value> maybe_value =
      base::JSONReader::Read(response_body);
  if (!maybe_value)
    return nullptr;

  return ParseSingleIssuer(std::move(*maybe_value));
}

std::unique_ptr<base::flat_map<SuitableTrustTokenOrigin,
                               mojom::TrustTokenKeyCommitmentResultPtr>>
TrustTokenKeyCommitmentParser::ParseMultipleIssuers(
    std::string_view response_body) {
  std::optional<base::Value> maybe_value =
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

  for (auto kv : maybe_value->GetDict()) {
    const std::string& raw_key_from_json = kv.first;
    std::optional<SuitableTrustTokenOrigin> maybe_issuer =
        SuitableTrustTokenOrigin::Create(GURL(raw_key_from_json));

    if (!maybe_issuer)
      continue;

    mojom::TrustTokenKeyCommitmentResultPtr commitment_result =
        ParseSingleIssuer(kv.second);

    if (!commitment_result)
      continue;

    parsed_entries.emplace_back(std::move(*maybe_issuer), raw_key_from_json,
                                std::move(commitment_result));
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
