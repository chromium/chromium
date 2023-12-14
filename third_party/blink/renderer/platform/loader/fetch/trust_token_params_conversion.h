// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_TRUST_TOKEN_PARAMS_CONVERSION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_TRUST_TOKEN_PARAMS_CONVERSION_H_

#include "services/network/public/cpp/optional_trust_token_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_common.h"

namespace network {
namespace mojom {
namespace blink {
class TrustTokenParams;

}  // namespace blink
}  // namespace mojom
}  // namespace network

namespace blink {

// Converts a mojom::blink TrustTokenParams object to its non-Blink counterpart
// by directly copying all fields, converting types where necessary.
network::OptionalTrustTokenParams ConvertTrustTokenParams(
    const absl::optional<network::mojom::blink::TrustTokenParams>& maybe_in);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_TRUST_TOKEN_PARAMS_CONVERSION_H_
