// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FINGERPRINTING_PROTECTION_NOISE_TOKEN_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FINGERPRINTING_PROTECTION_NOISE_TOKEN_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/fingerprinting_protection/noise_token.h"
#include "third_party/blink/public/mojom/fingerprinting_protection/noise_token.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::NoiseTokenDataView,
                                        blink::NoiseToken> {
  static uint64_t value(const blink::NoiseToken& token) {
    return token.Value();
  }

  static bool Read(blink::mojom::NoiseTokenDataView data,
                   blink::NoiseToken* out) {
    *out = blink::NoiseToken(data.value());
    return true;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FINGERPRINTING_PROTECTION_NOISE_TOKEN_MOJOM_TRAITS_H_
