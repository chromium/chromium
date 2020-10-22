// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_LOCAL_TRUST_TOKEN_OPERATION_DELEGATE_H_
#define SERVICES_NETWORK_TRUST_TOKENS_LOCAL_TRUST_TOKEN_OPERATION_DELEGATE_H_

#include "base/callback.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"

namespace network {

// LocalTrustTokenOperationDelegate provides an interface for executing
// Trust Tokens "against the platform," i.e. via some kind of operating system
// mediation rather than through the typical method of a direct HTTP request to
// a counterparty.
class LocalTrustTokenOperationDelegate {
 public:
  virtual ~LocalTrustTokenOperationDelegate() = default;

  // FulfillIssuance attempts to execute the given Trust Tokens operation
  // locally, on conclusion populating |done| with either a response or a
  // suitable status as described in FulfillTrustTokenIssuanceAnswer's struct
  // comments.
  virtual void FulfillIssuance(
      mojom::FulfillTrustTokenIssuanceRequestPtr request,
      base::OnceCallback<void(mojom::FulfillTrustTokenIssuanceAnswerPtr)>
          done) = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_LOCAL_TRUST_TOKEN_OPERATION_DELEGATE_H_
