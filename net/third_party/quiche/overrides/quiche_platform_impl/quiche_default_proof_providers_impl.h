// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_DEFAULT_PROOF_PROVIDERS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_DEFAULT_PROOF_PROVIDERS_IMPL_H_

#include <memory>

#include "net/third_party/quiche/src/quiche/quic/core/crypto/proof_source.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/proof_verifier.h"

namespace quiche {

std::unique_ptr<quic::ProofVerifier> CreateDefaultProofVerifierImpl(
    const std::string& host);
std::unique_ptr<quic::ProofSource> CreateDefaultProofSourceImpl();

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_DEFAULT_PROOF_PROVIDERS_IMPL_H_
