/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CRYPTO_KEY_ALGORITHM_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CRYPTO_KEY_ALGORITHM_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm_params.h"
#include "third_party/blink/public/platform/web_private_ptr.h"

#if INSIDE_BLINK
#include <memory>
#endif

namespace blink {

class WebCryptoKeyAlgorithmPrivate;

// WebCryptoKeyAlgorithm represents the algorithm used to generate a key.
//   * Immutable
//   * Threadsafe
//   * Copiable (cheaply)
class WebCryptoKeyAlgorithm {
 public:
  WebCryptoKeyAlgorithm() = default;

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebCryptoKeyAlgorithm(
      WebCryptoAlgorithmId,
      std::unique_ptr<WebCryptoKeyAlgorithmParams>);
#endif

  // FIXME: Delete this in favor of the Create*() functions.
  BLINK_PLATFORM_EXPORT static WebCryptoKeyAlgorithm AdoptParamsAndCreate(
      WebCryptoAlgorithmId,
      WebCryptoKeyAlgorithmParams*);

  BLINK_PLATFORM_EXPORT static WebCryptoKeyAlgorithm CreateAes(
      WebCryptoAlgorithmId,
      uint16_t key_length_bits);
  BLINK_PLATFORM_EXPORT static WebCryptoKeyAlgorithm CreateHmac(
      WebCryptoAlgorithmId hash,
      unsigned key_length_bits);
  BLINK_PLATFORM_EXPORT static WebCryptoKeyAlgorithm CreateRsaHashed(
      WebCryptoAlgorithmId,
      unsigned modulus_length_bits,
      const unsigned char* public_exponent,
      unsigned public_exponent_size,
      WebCryptoAlgorithmId hash);
  BLINK_PLATFORM_EXPORT static WebCryptoKeyAlgorithm CreateEc(
      WebCryptoAlgorithmId,
      WebCryptoNamedCurve);
  BLINK_PLATFORM_EXPORT static WebCryptoKeyAlgorithm CreateWithoutParams(
      WebCryptoAlgorithmId);

  ~WebCryptoKeyAlgorithm() { Reset(); }

  WebCryptoKeyAlgorithm(const WebCryptoKeyAlgorithm& other) { Assign(other); }
  WebCryptoKeyAlgorithm& operator=(const WebCryptoKeyAlgorithm& other) {
    Assign(other);
    return *this;
  }

  BLINK_PLATFORM_EXPORT bool IsNull() const;

  BLINK_PLATFORM_EXPORT WebCryptoAlgorithmId Id() const;

  BLINK_PLATFORM_EXPORT WebCryptoKeyAlgorithmParamsType ParamsType() const;

  // Returns the type-specific parameters for this key. If the requested
  // parameters are not applicable (for instance an HMAC key does not have
  // any AES parameters) then returns 0.
  BLINK_PLATFORM_EXPORT WebCryptoAesKeyAlgorithmParams* AesParams() const;
  BLINK_PLATFORM_EXPORT WebCryptoHmacKeyAlgorithmParams* HmacParams() const;
  BLINK_PLATFORM_EXPORT WebCryptoRsaHashedKeyAlgorithmParams* RsaHashedParams()
      const;
  BLINK_PLATFORM_EXPORT WebCryptoEcKeyAlgorithmParams* EcParams() const;

  // Write the algorithm parameters to a dictionary.
  BLINK_PLATFORM_EXPORT void WriteToDictionary(
      WebCryptoKeyAlgorithmDictionary*) const;

 private:
  BLINK_PLATFORM_EXPORT void Assign(const WebCryptoKeyAlgorithm& other);
  BLINK_PLATFORM_EXPORT void Reset();

  WebPrivatePtr<WebCryptoKeyAlgorithmPrivate> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CRYPTO_KEY_ALGORITHM_H_
