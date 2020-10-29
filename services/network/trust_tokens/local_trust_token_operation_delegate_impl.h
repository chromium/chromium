// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_LOCAL_TRUST_TOKEN_OPERATION_DELEGATE_IMPL_H_
#define SERVICES_NETWORK_TRUST_TOKENS_LOCAL_TRUST_TOKEN_OPERATION_DELEGATE_IMPL_H_

#include "base/callback.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/local_trust_token_operation_delegate.h"

namespace network {

// LocalTrustTokenOperationDelegateImpl provides an interface for executing
// Trust Tokens "against the platform," i.e. via some kind of operating system
// mediation rather than through the typical method of a direct HTTP request to
// a counterparty.
class LocalTrustTokenOperationDelegateImpl
    : public LocalTrustTokenOperationDelegate {
 public:
  // |client_provider| provides a handle to a NetworkContextClient that will be
  // used for requesting Trust Tokens operations' local execution.
  //
  // client_provider.Run() will be called before each attempt to delegate a
  // Trust Tokens operation. It is permitted to return nullptr; in this case,
  // the operation will be cancelled.
  explicit LocalTrustTokenOperationDelegateImpl(
      base::RepeatingCallback<mojom::NetworkContextClient*(void)>
          client_provider);

  ~LocalTrustTokenOperationDelegateImpl() override;

  // FulfillIssuance attempts to execute the given Trust Tokens operation
  // locally, on conclusion populating |done| with either a response or a
  // suitable status as described in FulfillTrustTokenIssuanceAnswer's struct
  // comments.
  void FulfillIssuance(
      mojom::FulfillTrustTokenIssuanceRequestPtr request,
      base::OnceCallback<void(mojom::FulfillTrustTokenIssuanceAnswerPtr)> done)
      override;

 private:
  base::RepeatingCallback<mojom::NetworkContextClient*(void)> client_provider_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_LOCAL_TRUST_TOKEN_OPERATION_DELEGATE_IMPL_H_
