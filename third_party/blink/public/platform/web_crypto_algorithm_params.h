/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CRYPTO_ALGORITHM_PARAMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CRYPTO_ALGORITHM_PARAMS_H_

#include "base/check.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

// NOTE: For documentation on the meaning of each of the parameters see the
//       Web crypto spec:
//
//       http://www.w3.org/TR/WebCryptoAPI
//
//       For the most part, the parameters in the spec have the same name,
//       except that in the blink code:
//
//         - Structure names are prefixed by "WebCrypto"
//         - Optional fields are prefixed by "optional"
//         - Data length properties are suffixed by either "Bits" or "Bytes"

class WebCryptoAlgorithmParams {
 public:
  WebCryptoAlgorithmParams() = default;
  virtual ~WebCryptoAlgorithmParams() = default;
  virtual WebCryptoAlgorithmParamsType GetType() const = 0;
};

class WebCryptoAesCbcParams : public WebCryptoAlgorithmParams {
 public:
  explicit WebCryptoAesCbcParams(WebVector<unsigned char> iv)
      : iv_(std::move(iv)) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeAesCbcParams;
  }

  const WebVector<unsigned char>& Iv() const { return iv_; }

 private:
  const WebVector<unsigned char> iv_;
};

class WebCryptoAlgorithmParamsWithHash : public WebCryptoAlgorithmParams {
 public:
  explicit WebCryptoAlgorithmParamsWithHash(const WebCryptoAlgorithm& hash)
      : hash_(hash) {
    DCHECK(!hash.IsNull());
  }

  const WebCryptoAlgorithm& GetHash() const { return hash_; }

 private:
  const WebCryptoAlgorithm hash_;
};

class WebCryptoAesCtrParams : public WebCryptoAlgorithmParams {
 public:
  WebCryptoAesCtrParams(unsigned char length_bits,
                        WebVector<unsigned char> counter)
      : WebCryptoAlgorithmParams(),
        counter_(std::move(counter)),
        length_bits_(length_bits) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeAesCtrParams;
  }

  const WebVector<unsigned char>& Counter() const { return counter_; }
  unsigned char LengthBits() const { return length_bits_; }

 private:
  const WebVector<unsigned char> counter_;
  const unsigned char length_bits_;
};

class WebCryptoAesKeyGenParams : public WebCryptoAlgorithmParams {
 public:
  explicit WebCryptoAesKeyGenParams(uint16_t length_bits)
      : length_bits_(length_bits) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeAesKeyGenParams;
  }

  uint16_t LengthBits() const { return length_bits_; }

 private:
  const uint16_t length_bits_;
};

class WebCryptoHmacImportParams : public WebCryptoAlgorithmParamsWithHash {
 public:
  // FIXME: Remove this constructor once it is no longer used by Chromium.
  // http://crbug.com/431085
  explicit WebCryptoHmacImportParams(const WebCryptoAlgorithm& hash)
      : WebCryptoAlgorithmParamsWithHash(hash),
        has_length_bits_(false),
        optional_length_bits_(0) {}

  WebCryptoHmacImportParams(const WebCryptoAlgorithm& hash,
                            bool has_length_bits,
                            unsigned length_bits)
      : WebCryptoAlgorithmParamsWithHash(hash),
        has_length_bits_(has_length_bits),
        optional_length_bits_(length_bits) {
    DCHECK(has_length_bits || !length_bits);
  }

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeHmacImportParams;
  }

  bool HasLengthBits() const { return has_length_bits_; }

  unsigned OptionalLengthBits() const { return optional_length_bits_; }

 private:
  const bool has_length_bits_;
  const unsigned optional_length_bits_;
};

class WebCryptoHmacKeyGenParams : public WebCryptoAlgorithmParamsWithHash {
 public:
  WebCryptoHmacKeyGenParams(const WebCryptoAlgorithm& hash,
                            bool has_length_bits,
                            unsigned length_bits)
      : WebCryptoAlgorithmParamsWithHash(hash),
        has_length_bits_(has_length_bits),
        optional_length_bits_(length_bits) {
    DCHECK(has_length_bits || !length_bits);
  }

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeHmacKeyGenParams;
  }

  bool HasLengthBits() const { return has_length_bits_; }

  unsigned OptionalLengthBits() const { return optional_length_bits_; }

 private:
  const bool has_length_bits_;
  const unsigned optional_length_bits_;
};

class WebCryptoAesGcmParams : public WebCryptoAlgorithmParams {
 public:
  WebCryptoAesGcmParams(WebVector<unsigned char> iv,
                        bool has_additional_data,
                        WebVector<unsigned char> additional_data,
                        bool has_tag_length_bits,
                        unsigned char tag_length_bits)
      : iv_(std::move(iv)),
        has_additional_data_(has_additional_data),
        optional_additional_data_(std::move(additional_data)),
        has_tag_length_bits_(has_tag_length_bits),
        optional_tag_length_bits_(tag_length_bits) {
    DCHECK(has_additional_data || optional_additional_data_.empty());
    DCHECK(has_tag_length_bits || !tag_length_bits);
  }

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeAesGcmParams;
  }

  const WebVector<unsigned char>& Iv() const { return iv_; }

  bool HasAdditionalData() const { return has_additional_data_; }
  const WebVector<unsigned char>& OptionalAdditionalData() const {
    return optional_additional_data_;
  }

  bool HasTagLengthBits() const { return has_tag_length_bits_; }
  unsigned OptionalTagLengthBits() const { return optional_tag_length_bits_; }

 private:
  const WebVector<unsigned char> iv_;
  const bool has_additional_data_;
  const WebVector<unsigned char> optional_additional_data_;
  const bool has_tag_length_bits_;
  const unsigned char optional_tag_length_bits_;
};

class WebCryptoRsaHashedImportParams : public WebCryptoAlgorithmParamsWithHash {
 public:
  explicit WebCryptoRsaHashedImportParams(const WebCryptoAlgorithm& hash)
      : WebCryptoAlgorithmParamsWithHash(hash) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeRsaHashedImportParams;
  }
};

class WebCryptoRsaHashedKeyGenParams : public WebCryptoAlgorithmParams {
 public:
  WebCryptoRsaHashedKeyGenParams(const WebCryptoAlgorithm& hash,
                                 unsigned modulus_length_bits,
                                 WebVector<unsigned char> public_exponent)
      : modulus_length_bits_(modulus_length_bits),
        public_exponent_(std::move(public_exponent)),
        hash_(hash) {
    DCHECK(!hash.IsNull());
  }

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams;
  }

  unsigned ModulusLengthBits() const { return modulus_length_bits_; }
  const WebVector<unsigned char>& PublicExponent() const {
    return public_exponent_;
  }
  const WebCryptoAlgorithm& GetHash() const { return hash_; }

  // Converts the public exponent (big-endian WebCrypto BigInteger),
  // with or without leading zeros, to unsigned int. Returns true on success.
  bool ConvertPublicExponentToUnsigned(unsigned& result) const {
    result = 0;
    for (size_t i = 0; i < public_exponent_.size(); ++i) {
      size_t i_reversed = public_exponent_.size() - i - 1;

      if (i_reversed >= sizeof(result) && public_exponent_[i])
        return false;  // Too large for unsigned int.

      result |= public_exponent_[i] << 8 * i_reversed;
    }
    return true;
  }

 private:
  const unsigned modulus_length_bits_;
  const WebVector<unsigned char> public_exponent_;
  const WebCryptoAlgorithm hash_;
};

class WebCryptoRsaOaepParams : public WebCryptoAlgorithmParams {
 public:
  WebCryptoRsaOaepParams(bool has_label, WebVector<unsigned char> label)
      : has_label_(has_label), optional_label_(std::move(label)) {
    DCHECK(has_label || optional_label_.empty());
  }

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeRsaOaepParams;
  }

  bool HasLabel() const { return has_label_; }
  const WebVector<unsigned char>& OptionalLabel() const {
    return optional_label_;
  }

 private:
  const bool has_label_;
  const WebVector<unsigned char> optional_label_;
};

class WebCryptoRsaPssParams : public WebCryptoAlgorithmParams {
 public:
  explicit WebCryptoRsaPssParams(unsigned salt_length_bytes)
      : salt_length_bytes_(salt_length_bytes) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeRsaPssParams;
  }

  unsigned SaltLengthBytes() const { return salt_length_bytes_; }

 private:
  const unsigned salt_length_bytes_;
};

class WebCryptoEcdsaParams : public WebCryptoAlgorithmParamsWithHash {
 public:
  explicit WebCryptoEcdsaParams(const WebCryptoAlgorithm& hash)
      : WebCryptoAlgorithmParamsWithHash(hash) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeEcdsaParams;
  }
};

class WebCryptoEcKeyGenParams : public WebCryptoAlgorithmParams {
 public:
  explicit WebCryptoEcKeyGenParams(WebCryptoNamedCurve named_curve)
      : named_curve_(named_curve) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeEcKeyGenParams;
  }

  WebCryptoNamedCurve NamedCurve() const { return named_curve_; }

 private:
  const WebCryptoNamedCurve named_curve_;
};

class WebCryptoEcKeyImportParams : public WebCryptoAlgorithmParams {
 public:
  explicit WebCryptoEcKeyImportParams(WebCryptoNamedCurve named_curve)
      : named_curve_(named_curve) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeEcKeyImportParams;
  }

  WebCryptoNamedCurve NamedCurve() const { return named_curve_; }

 private:
  const WebCryptoNamedCurve named_curve_;
};

class WebCryptoEcdhKeyDeriveParams : public WebCryptoAlgorithmParams {
 public:
  explicit WebCryptoEcdhKeyDeriveParams(const WebCryptoKey& public_key)
      : public_key_(public_key) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeEcdhKeyDeriveParams;
  }

  const WebCryptoKey& PublicKey() const { return public_key_; }

 private:
  const WebCryptoKey public_key_;
};

class WebCryptoAesDerivedKeyParams : public WebCryptoAlgorithmParams {
 public:
  explicit WebCryptoAesDerivedKeyParams(uint16_t length_bits)
      : length_bits_(length_bits) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeAesDerivedKeyParams;
  }

  uint16_t LengthBits() const { return length_bits_; }

 private:
  const uint16_t length_bits_;
};

class WebCryptoHkdfParams : public WebCryptoAlgorithmParamsWithHash {
 public:
  WebCryptoHkdfParams(const WebCryptoAlgorithm& hash,
                      WebVector<unsigned char> salt,
                      WebVector<unsigned char> info)
      : WebCryptoAlgorithmParamsWithHash(hash),
        salt_(std::move(salt)),
        info_(std::move(info)) {}

  const WebVector<unsigned char>& Salt() const { return salt_; }

  const WebVector<unsigned char>& Info() const { return info_; }

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeHkdfParams;
  }

 private:
  const WebVector<unsigned char> salt_;
  const WebVector<unsigned char> info_;
};

class WebCryptoPbkdf2Params : public WebCryptoAlgorithmParamsWithHash {
 public:
  WebCryptoPbkdf2Params(const WebCryptoAlgorithm& hash,
                        WebVector<unsigned char> salt,
                        unsigned iterations)
      : WebCryptoAlgorithmParamsWithHash(hash),
        salt_(std::move(salt)),
        iterations_(iterations) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypePbkdf2Params;
  }

  const WebVector<unsigned char>& Salt() const { return salt_; }
  unsigned Iterations() const { return iterations_; }

 private:
  const WebVector<unsigned char> salt_;
  const unsigned iterations_;
};

class WebCryptoEd25519Params : public WebCryptoAlgorithmParamsWithHash {
 public:
  explicit WebCryptoEd25519Params(const WebCryptoAlgorithm& hash)
      : WebCryptoAlgorithmParamsWithHash(hash) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeEd25519Params;
  }
};

class WebCryptoX25519KeyDeriveParams : public WebCryptoAlgorithmParams {
 public:
  explicit WebCryptoX25519KeyDeriveParams(const WebCryptoKey& public_key)
      : public_key_(public_key) {}

  WebCryptoAlgorithmParamsType GetType() const override {
    return kWebCryptoAlgorithmParamsTypeX25519KeyDeriveParams;
  }

  const WebCryptoKey& PublicKey() const { return public_key_; }

 private:
  const WebCryptoKey public_key_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CRYPTO_ALGORITHM_PARAMS_H_
