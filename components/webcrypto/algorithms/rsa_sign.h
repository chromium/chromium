// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_RSA_SIGN_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_RSA_SIGN_H_

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"

namespace blink {
class WebCryptoKey;
}

namespace webcrypto {

class Status;

// Helper functions for doing RSA-SSA signing and verification
// (both PKCS1-v1_5 and PSS flavor).
//
// The salt length parameter is only relevant when the key is for RSA-PSS. In
// other cases it should be set to zero.

Status RsaSign(const blink::WebCryptoKey& key,
               unsigned int pss_salt_length_bytes,
               base::span<const uint8_t> data,
               std::vector<uint8_t>* buffer);

Status RsaVerify(const blink::WebCryptoKey& key,
                 unsigned int pss_salt_length_bytes,
                 base::span<const uint8_t> signature,
                 base::span<const uint8_t> data,
                 bool* signature_match);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_RSA_SIGN_H_
