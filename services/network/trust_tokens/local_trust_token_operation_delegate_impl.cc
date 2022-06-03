// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/local_trust_token_operation_delegate_impl.h"

namespace network {

LocalTrustTokenOperationDelegateImpl::LocalTrustTokenOperationDelegateImpl(
    base::RepeatingCallback<mojom::NetworkContextClient*(void)> client_provider)
    : client_provider_(std::move(client_provider)) {}

LocalTrustTokenOperationDelegateImpl::~LocalTrustTokenOperationDelegateImpl() =
    default;

void LocalTrustTokenOperationDelegateImpl::FulfillIssuance(
    mojom::FulfillTrustTokenIssuanceRequestPtr request,
    base::OnceCallback<void(mojom::FulfillTrustTokenIssuanceAnswerPtr)> done) {
  mojom::NetworkContextClient* client = client_provider_.Run();
  if (!client) {
    auto answer = mojom::FulfillTrustTokenIssuanceAnswer::New();
    answer->status = mojom::FulfillTrustTokenIssuanceAnswer::Status::kNotFound;
    std::move(done).Run(std::move(answer));
    return;
  }

  client->OnTrustTokenIssuanceDivertedToSystem(std::move(request),
                                               std::move(done));
}

}  // namespace network
