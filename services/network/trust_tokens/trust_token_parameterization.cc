// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_parameterization.h"

#include "services/network/public/mojom/trust_tokens.mojom-forward.h"

namespace network {

size_t TrustTokenMaxKeysForVersion(mojom::TrustTokenProtocolVersion version) {
  switch (version) {
    case mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb:
    case mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Pmb:
      return 3;
    case mojom::TrustTokenProtocolVersion::kTrustTokenV3Voprf:
    case mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Voprf:
      return 6;
  }
}

}  // namespace network
