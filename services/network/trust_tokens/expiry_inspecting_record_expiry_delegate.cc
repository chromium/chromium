// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/expiry_inspecting_record_expiry_delegate.h"

#include "base/containers/contains.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/types.h"

namespace network {

ExpiryInspectingRecordExpiryDelegate::ExpiryInspectingRecordExpiryDelegate(
    const SynchronousTrustTokenKeyCommitmentGetter* key_commitment_getter)
    : key_commitment_getter_(key_commitment_getter) {}

bool ExpiryInspectingRecordExpiryDelegate::IsRecordExpired(
    const TrustTokenRedemptionRecord& record,
    const base::TimeDelta& time_since_last_redemption,
    const SuitableTrustTokenOrigin& issuer) {
  mojom::TrustTokenKeyCommitmentResultPtr key_commitments =
      key_commitment_getter_->GetSync(issuer.origin());

  // If there are no current key commitments for the issuer, either the record
  // changed (due to data corruption) or the commitments changed (due to an
  // overwrite by the key commitments' producer, or due to data corruption).
  //
  // In both cases, the RR's associated token-issuance verification key isn't
  // present in the current key commitments we possess for the issuer, because
  // we don't have any key commitments whatsoever for the issuer: mark the
  // record as expired.
  if (!key_commitments)
    return true;

  // Treat the RR as expired if its associated token-issuance verification key
  // (|token_verification_key|) is no longer present in its issuer's key
  // commitment.
  if (!base::Contains(key_commitments->keys, record.token_verification_key(),
                      &mojom::TrustTokenVerificationKey::body)) {
    return true;
  }

  if (record.has_lifetime() && !time_since_last_redemption.is_negative() &&
      static_cast<uint64_t>(time_since_last_redemption.InSeconds()) >=
          record.lifetime()) {
    return true;
  }

  return false;
}

}  // namespace network
