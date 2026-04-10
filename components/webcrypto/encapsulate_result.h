// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ENCAPSULATE_RESULT_H_
#define COMPONENTS_WEBCRYPTO_ENCAPSULATE_RESULT_H_

#include <vector>

#include "third_party/blink/public/platform/web_crypto.h"

namespace webcrypto {

// This is the result object when performing key encapsulation.
class EncapsulateKeyResult {
 public:
  EncapsulateKeyResult();
  ~EncapsulateKeyResult();

  bool IsNull() const;

  const blink::WebCryptoKey& shared_key() const;
  const std::vector<uint8_t>& ciphertext() const;

  void Assign(const blink::WebCryptoKey& shared_key,
              std::vector<uint8_t> ciphertext);

  // Sends the results to the Blink result. Should not be called for "null"
  // results.
  void Complete(blink::WebCryptoResult* out) const;

 private:
  bool is_null_ = true;

  blink::WebCryptoKey shared_key_;
  std::vector<uint8_t> ciphertext_;
};

// This is the result object when performing bits encapsulation.
class EncapsulateBitsResult {
 public:
  EncapsulateBitsResult();
  ~EncapsulateBitsResult();

  bool IsNull() const;

  const std::vector<uint8_t>& shared_bits() const;
  const std::vector<uint8_t>& ciphertext() const;

  void Assign(std::vector<uint8_t> shared_bits,
              std::vector<uint8_t> ciphertext);

  // Sends the results to the Blink result. Should not be called for "null"
  // results.
  void Complete(blink::WebCryptoResult* out) const;

 private:
  bool is_null_ = true;

  std::vector<uint8_t> shared_bits_;
  std::vector<uint8_t> ciphertext_;
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ENCAPSULATE_RESULT_H_
