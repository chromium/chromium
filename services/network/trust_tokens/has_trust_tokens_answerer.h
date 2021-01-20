// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_HAS_TRUST_TOKENS_ANSWERER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_HAS_TRUST_TOKENS_ANSWERER_H_

#include <memory>

#include "base/callback.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "url/origin.h"

namespace network {

// HasTrustTokensAnswerer is a class bound to a top-level origin, able to answer
// queries about whether the user possesses trust tokens issued by any issuer
// origin associated with the top-level origin.
//
// When receiving a HasTrustTokens(issuer) call, the answerer attempts to
// associate |issuer| with the bound top-level origin; if this is not possible,
// it returns an error (see the HasTrustTokensAnswerer Mojo interface's comment
// for all possible error returns).
class HasTrustTokensAnswerer : public mojom::HasTrustTokensAnswerer {
 public:
  // Constructs a new answerer bound to the given top frame origin.
  HasTrustTokensAnswerer(SuitableTrustTokenOrigin top_frame_origin,
                         PendingTrustTokenStore* pending_trust_token_store);

  ~HasTrustTokensAnswerer() override;

  HasTrustTokensAnswerer(const HasTrustTokensAnswerer&) = delete;
  HasTrustTokensAnswerer& operator=(const HasTrustTokensAnswerer&) = delete;

  // mojom::HasTrustTokensAnswerer:
  void HasTrustTokens(const url::Origin& issuer,
                      HasTrustTokensCallback callback) override;

 private:
  // Continuation of HasTrustTokens: uses |trust_token_store| to answer a
  // HasTrusttokens query against |issuer|.
  //
  // Requires that |issuer| is potentially trustworthy and HTTP or HTTPS.
  void AnswerQueryWithStore(const SuitableTrustTokenOrigin& issuer,
                            HasTrustTokensCallback callback,
                            TrustTokenStore* trust_token_store);

  const SuitableTrustTokenOrigin top_frame_origin_;
  PendingTrustTokenStore* pending_trust_token_store_;

  base::WeakPtrFactory<HasTrustTokensAnswerer> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_HAS_TRUST_TOKENS_ANSWERER_H_
