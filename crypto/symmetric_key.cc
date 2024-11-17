// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/symmetric_key.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "crypto/kdf.h"
#include "crypto/openssl_util.h"
#include "crypto/random.h"

namespace crypto {

namespace {

bool IsValidKeySize(size_t key_size_in_bytes) {
  // Nobody should ever be using other symmetric key sizes without consulting
  // with CRYPTO_OWNERS first, who can modify this check if need be.
  return key_size_in_bytes == 16 || key_size_in_bytes == 32;
}

}  // namespace

SymmetricKey::SymmetricKey(base::span<const uint8_t> key_bytes)
    : key_(base::as_string_view(key_bytes)) {}

SymmetricKey::SymmetricKey(const SymmetricKey& other) = default;
SymmetricKey& SymmetricKey::operator=(const SymmetricKey& other) = default;

SymmetricKey::~SymmetricKey() {
  std::fill(key_.begin(), key_.end(), '\0');  // Zero out the confidential key.
}

// static
std::unique_ptr<SymmetricKey> SymmetricKey::GenerateRandomKey(
    Algorithm,
    size_t key_size_in_bits) {
  return std::make_unique<SymmetricKey>(RandomKey(key_size_in_bits));
}

// static
SymmetricKey SymmetricKey::RandomKey(size_t key_size_in_bits) {
  CHECK(!(key_size_in_bits % 8));

  const size_t key_size_in_bytes = key_size_in_bits / 8;
  CHECK(IsValidKeySize(key_size_in_bytes));

  return SymmetricKey(crypto::RandBytesAsVector(key_size_in_bytes));
}

// static
std::unique_ptr<SymmetricKey> SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
    Algorithm,
    const std::string& password,
    const std::string& salt,
    size_t iterations,
    size_t key_size_in_bits) {
  if ((key_size_in_bits % 8) || !IsValidKeySize(key_size_in_bits / 8)) {
    return nullptr;
  }

  kdf::Pbkdf2HmacSha1Params params = {
      .iterations = base::checked_cast<decltype(params.iterations)>(iterations),
  };

  std::vector<uint8_t> key(key_size_in_bits / 8);
  kdf::DeriveKeyPbkdf2HmacSha1(params, base::as_byte_span(password),
                               base::as_byte_span(salt), key, SubtlePassKey{});

  return std::make_unique<SymmetricKey>(key);
}

// static
std::unique_ptr<SymmetricKey> SymmetricKey::DeriveKeyFromPasswordUsingScrypt(
    Algorithm,
    const std::string& password,
    const std::string& salt,
    size_t cost_parameter,
    size_t block_size,
    size_t parallelization_parameter,
    size_t max_memory_bytes,
    size_t key_size_in_bits) {
  if ((key_size_in_bits % 8) || !IsValidKeySize(key_size_in_bits / 8)) {
    return nullptr;
  }

  kdf::ScryptParams params = {
      .cost = cost_parameter,
      .block_size = block_size,
      .parallelization = parallelization_parameter,
      .max_memory_bytes = max_memory_bytes,
  };
  std::vector<uint8_t> key(key_size_in_bits / 8);
  kdf::DeriveKeyScrypt(params, base::as_byte_span(password),
                       base::as_byte_span(salt), key, SubtlePassKey{});
  return std::make_unique<SymmetricKey>(key);
}

// static
std::unique_ptr<SymmetricKey> SymmetricKey::Import(Algorithm,
                                                   const std::string& raw_key) {
  if (!IsValidKeySize(raw_key.size())) {
    return nullptr;
  }
  return std::make_unique<SymmetricKey>(base::as_byte_span(raw_key));
}

}  // namespace crypto
