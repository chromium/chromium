// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_EXPIRY_INSPECTING_RECORD_EXPIRY_DELEGATE_H_
#define SERVICES_NETWORK_TRUST_TOKENS_EXPIRY_INSPECTING_RECORD_EXPIRY_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_store.h"

namespace network {

class SynchronousTrustTokenKeyCommitmentGetter;

// ExpiryInspectingRecordExpiryDelegate considers a redemption record (RR) to
// have expired if:
// - its associated trust token verification key is not present in the token's
// issuer's most recent key commitment, or
// - its issuer is not a valid Trust Tokens issuer (this means the record has
// been corrupted; signaling that it is expired increases the likelihood that
// the caller will evict it from storage).
class ExpiryInspectingRecordExpiryDelegate
    : public TrustTokenStore::RecordExpiryDelegate {
 public:
  // Constructs a new expiry delegate using |key_commitment_getter|, which must
  // outlive this object, to inspect the most recent key commitment of each RR.
  explicit ExpiryInspectingRecordExpiryDelegate(
      const SynchronousTrustTokenKeyCommitmentGetter* key_commitment_getter);

  ExpiryInspectingRecordExpiryDelegate(
      const ExpiryInspectingRecordExpiryDelegate&) = delete;
  ExpiryInspectingRecordExpiryDelegate& operator=(
      const ExpiryInspectingRecordExpiryDelegate&) = delete;

  // TrustTokenStore::RecordExpiryDelegate implementation:
  bool IsRecordExpired(const TrustTokenRedemptionRecord& record,
                       const base::TimeDelta& time_since_last_redemption,
                       const SuitableTrustTokenOrigin& issuer) override;

 private:
  raw_ptr<const SynchronousTrustTokenKeyCommitmentGetter>
      key_commitment_getter_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_EXPIRY_INSPECTING_RECORD_EXPIRY_DELEGATE_H_
