// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_DEFAULT_PROOF_PROVIDERS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_DEFAULT_PROOF_PROVIDERS_IMPL_H_

#include <memory>

#include "net/third_party/quiche/src/quic/core/crypto/proof_source.h"
#include "net/third_party/quiche/src/quic/core/crypto/proof_verifier.h"

namespace quic {

std::unique_ptr<ProofVerifier> CreateDefaultProofVerifierImpl(
    const std::string& host);
std::unique_ptr<ProofSource> CreateDefaultProofSourceImpl();

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_DEFAULT_PROOF_PROVIDERS_IMPL_H_
