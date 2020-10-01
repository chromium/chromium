// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitment_parser.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"

namespace network {

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
  if (*maybe_version == "TrustTokenV1") {
    result->protocol_version = mojom::TrustTokenProtocolVersion::kTrustTokenV1;
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

  // Confirm that the srrkey field is present and base64-encoded.
  const std::string* maybe_srrkey =
      value.FindStringKey(kTrustTokenKeyCommitmentSrrkeyField);
  if (!maybe_srrkey)
    return nullptr;
  if (!base::Base64Decode(*maybe_srrkey,
                          &result->signed_redemption_record_verification_key)) {
    return nullptr;
  }

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

const char kTrustTokenKeyCommitmentProtocolVersionField[] = "protocol_version";
const char kTrustTokenKeyCommitmentIDField[] = "id";
const char kTrustTokenKeyCommitmentBatchsizeField[] = "batchsize";
const char kTrustTokenKeyCommitmentSrrkeyField[] = "srrkey";
const char kTrustTokenKeyCommitmentExpiryField[] = "expiry";
const char kTrustTokenKeyCommitmentKeyField[] = "Y";

// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#bookmark=id.6wh9crbxdizi
// {
//   "protocol_version" : ..., // Protocol Version; value of type string.
//   "id" : ...,               // ID; value of type int.
//   "batchsize" : ...,        // Batch size; value of type int.
//   "srrkey" : ...,           // Required Signed Redemption Record (SRR)
//                             // verification key, in base64.
//
//   "1" : {                   // Key label, a number in uint32_t range; ignored
//                             // except for checking that it is present and
//                             // type-safe.
//     "Y" : ...,              // Required token issuance verification key, in
//                             // base64.
//     "expiry" : ...,         // Required token issuance key expiry time, in
//                             // microseconds since the Unix epoch.
//   },
//   "17" : {                  // No guarantee that key labels (1, 7) are dense.
//     "Y" : ...,
//     "expiry" : ...,
//   }
// }
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
