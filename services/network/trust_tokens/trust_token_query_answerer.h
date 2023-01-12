// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_QUERY_ANSWERER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_QUERY_ANSWERER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "url/origin.h"

namespace network {

class SynchronousTrustTokenKeyCommitmentGetter;

// TrustTokenQueryAnswerer is a class bound to a top-level origin, able to
// answer queries about whether the user possesses trust tokens and redemption
// records issued by any issuer origin associated with the top-level origin.
//
// When receiving a HasTrustTokens(issuer) or HasRedemptionRecord(issuer) call,
// the answerer attempts to associate |issuer| with the bound top-level origin;
// if this is not possible, it returns an error (see the TrustTokenQueryAnswerer
// Mojo interface's comment for all possible error returns).
class TrustTokenQueryAnswerer : public mojom::TrustTokenQueryAnswerer {
 public:
  // Constructs a new answerer bound to the given top frame origin.
  TrustTokenQueryAnswerer(
      SuitableTrustTokenOrigin top_frame_origin,
      PendingTrustTokenStore* pending_trust_token_store,
      const SynchronousTrustTokenKeyCommitmentGetter* key_commitment_getter);

  ~TrustTokenQueryAnswerer() override;

  TrustTokenQueryAnswerer(const TrustTokenQueryAnswerer&) = delete;
  TrustTokenQueryAnswerer& operator=(const TrustTokenQueryAnswerer&) = delete;

  // mojom::TrustTokenQueryAnswerer::HasTrustTokens
  void HasTrustTokens(const url::Origin& issuer,
                      HasTrustTokensCallback callback) override;

  // mojom::TrustTokenQueryAnswerer::HasRedemptionRecord
  void HasRedemptionRecord(const url::Origin& issuer,
                           HasRedemptionRecordCallback callback) override;

 private:
  // Continuation of HasTrustTokens: uses |trust_token_store| to answer a
  // HasTrustTokens query against |issuer|.
  //
  // Requires that |issuer| is potentially trustworthy and HTTP or HTTPS.
  void AnswerTokenQueryWithStore(const SuitableTrustTokenOrigin& issuer,
                                 HasTrustTokensCallback callback,
                                 TrustTokenStore* trust_token_store) const;

  // Continuation of HasRedemptionRecord: uses |trust_token_store| to answer a
  // HasRedemptionRecord query against |issuer|.
  //
  // Requires that |issuer| is potentially trustworthy and HTTP or HTTPS.
  void AnswerRedemptionQueryWithStore(const SuitableTrustTokenOrigin& issuer,
                                      HasRedemptionRecordCallback callback,
                                      TrustTokenStore* trust_token_store) const;

  const SuitableTrustTokenOrigin top_frame_origin_;
  raw_ptr<PendingTrustTokenStore> pending_trust_token_store_;
  raw_ptr<const SynchronousTrustTokenKeyCommitmentGetter> const
      key_commitment_getter_;

  base::WeakPtrFactory<TrustTokenQueryAnswerer> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_QUERY_ANSWERER_H_
