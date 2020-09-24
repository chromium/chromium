// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/expiry_inspecting_record_expiry_delegate.h"

#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/signed_redemption_record_serialization.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/types.h"

namespace network {

namespace {

const char kExpiryTimestampKey[] = "expiry-timestamp";

// Extracts the expiry timestamp from the given SRR body, a top-level CBOR map
// member of type integer with key "expiry-timestamp" and value an expiry in
// seconds since the UNIX epoch. (The Trust Tokens design doc is the current
// normative source for this field's format.)
//
// Returns |kTrustTokenDefaultRedemptionRecordExpiry| if the optional timestamp
// is not present in the SRR body; returns nullopt if the SRR body is not a map,
// or if it contains an "expiry-timestamp" member that is not an integer.
//
// Implementation note: We do this on the fly since it is pretty fast (a few
// microseconds) and expected to be done at most once per request.
// Pre-extracting the timestamp and storing it alongside the SRR would be a
// potential improvement if the control flow changes so that multiple SRRs are
// queried for expiry per protocol operation, but would introduce extra
// complexity: the SRRs could get out of sync with the timestamp bodies.
base::Optional<base::Time> ExtractExpiryTimestampOrDefault(
    base::span<const uint8_t> srr_body) {
  // Extract the expiry timestamp now so that downstream consumers don't have to
  // deserialize CBOR repeatedly to inspect the timestamp.
  //
  // From the design doc:
  //   “expiry-timestamp”: <optional expiry timestamp, seconds past the Unix
  //   epoch>
  base::Optional<cbor::Value> maybe_cbor = cbor::Reader::Read(srr_body);
  if (!maybe_cbor || !maybe_cbor->is_map())
    return base::nullopt;

  cbor::Value expiry_timestamp_cbor_key(kExpiryTimestampKey,
                                        cbor::Value::Type::STRING);

  auto it = maybe_cbor->GetMap().find(expiry_timestamp_cbor_key);
  if (it == maybe_cbor->GetMap().end())
    return kTrustTokenDefaultRedemptionRecordExpiry;
  if (!it->second.is_integer())
    return base::nullopt;

  return base::Time::UnixEpoch() +
         base::TimeDelta::FromSeconds(it->second.GetInteger());
}

}  // namespace

ExpiryInspectingRecordExpiryDelegate::ExpiryInspectingRecordExpiryDelegate(
    const SynchronousTrustTokenKeyCommitmentGetter* key_commitment_getter)
    : key_commitment_getter_(key_commitment_getter) {}

bool ExpiryInspectingRecordExpiryDelegate::IsRecordExpired(
    const SignedTrustTokenRedemptionRecord& record,
    const SuitableTrustTokenOrigin& issuer) {
  std::string record_body;
  if (!ParseTrustTokenSignedRedemptionRecord(record.body(), &record_body,
                                             /*signature_out=*/nullptr)) {
    // Malformed record; say that it's expired so it gets deleted.
    return true;
  }

  base::Optional<base::Time> expiry_timestamp_or_error =
      ExtractExpiryTimestampOrDefault(
          base::as_bytes(base::make_span(record_body)));
  if (!expiry_timestamp_or_error) {
    // Malformed record; say that it's expired so it gets deleted.
    return true;
  }

  if (*expiry_timestamp_or_error <= base::Time::Now())
    return true;

  mojom::TrustTokenKeyCommitmentResultPtr key_commitments =
      key_commitment_getter_->GetSync(issuer.origin());

  // If there are no current key commitments for the issuer, either the record
  // changed (due to data corruption) or the commitments changed (due to an
  // overwrite by the key commitments' producer, or due to data corruption).
  //
  // In both cases, the SRR's associated token-issuance verification key isn't
  // present in the current key commitments we possess for the issuer, because
  // we don't have any key commitments whatsoever for the issuer: mark the
  // record as expired.
  if (!key_commitments)
    return true;

  // Treat the SRR as expired if its associated token-issuance verification key
  // (|token_verification_key|) is no longer present in its issuer's key
  // commitment.
  if (!std::any_of(key_commitments->keys.begin(), key_commitments->keys.end(),
                   [&record](const mojom::TrustTokenVerificationKeyPtr& key) {
                     return key->body == record.token_verification_key();
                   })) {
    return true;
  }

  return false;
}

}  // namespace network
