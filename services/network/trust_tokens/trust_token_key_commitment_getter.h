// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_GETTER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_GETTER_H_

#include "base/functional/callback.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "url/origin.h"

namespace network {

// Class TrustTokenKeyCommitmentGetter fetches key commitments asynchronously.
// These are used for precondition checking before issuance, and for validating
// received tokens in issuance responses.
class TrustTokenKeyCommitmentGetter {
 public:
  virtual ~TrustTokenKeyCommitmentGetter() = default;
  virtual void Get(
      const url::Origin& origin,
      base::OnceCallback<void(mojom::TrustTokenKeyCommitmentResultPtr)> on_done)
      const = 0;
};

// Class SynchronousTrustTokenKeyCommitmentGetter fetches key commitments
// synchronously.
class SynchronousTrustTokenKeyCommitmentGetter {
 public:
  virtual ~SynchronousTrustTokenKeyCommitmentGetter() = default;
  virtual mojom::TrustTokenKeyCommitmentResultPtr GetSync(
      const url::Origin& origin) const = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_GETTER_H_
