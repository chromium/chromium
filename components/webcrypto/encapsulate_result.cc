// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/encapsulate_result.h"

#include "base/check.h"

namespace webcrypto {

EncapsulateKeyResult::EncapsulateKeyResult() = default;

EncapsulateKeyResult::~EncapsulateKeyResult() = default;

bool EncapsulateKeyResult::IsNull() const {
  return is_null_;
}

const blink::WebCryptoKey& EncapsulateKeyResult::shared_key() const {
  DCHECK(!is_null_);
  return shared_key_;
}

const std::vector<uint8_t>& EncapsulateKeyResult::ciphertext() const {
  DCHECK(!is_null_);
  return ciphertext_;
}

void EncapsulateKeyResult::Assign(const blink::WebCryptoKey& shared_key,
                                  std::vector<uint8_t> ciphertext) {
  DCHECK(is_null_);
  is_null_ = false;
  shared_key_ = shared_key;
  ciphertext_ = std::move(ciphertext);
}

void EncapsulateKeyResult::Complete(blink::WebCryptoResult* out) const {
  DCHECK(!is_null_);
  out->CompleteWithEncapsulatedKey(shared_key_, ciphertext_);
}

EncapsulateBitsResult::EncapsulateBitsResult() = default;

EncapsulateBitsResult::~EncapsulateBitsResult() = default;

bool EncapsulateBitsResult::IsNull() const {
  return is_null_;
}

const std::vector<uint8_t>& EncapsulateBitsResult::shared_bits() const {
  DCHECK(!is_null_);
  return shared_bits_;
}

const std::vector<uint8_t>& EncapsulateBitsResult::ciphertext() const {
  DCHECK(!is_null_);
  return ciphertext_;
}

void EncapsulateBitsResult::Assign(std::vector<uint8_t> shared_bits,
                                   std::vector<uint8_t> ciphertext) {
  DCHECK(is_null_);
  is_null_ = false;
  shared_bits_ = std::move(shared_bits);
  ciphertext_ = std::move(ciphertext);
}

void EncapsulateBitsResult::Complete(blink::WebCryptoResult* out) const {
  DCHECK(!is_null_);
  out->CompleteWithEncapsulatedBits(shared_bits_, ciphertext_);
}

}  // namespace webcrypto
