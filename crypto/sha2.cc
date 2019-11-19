// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/sha2.h"

#include <stddef.h>

#include <memory>

#include "base/stl_util.h"
#include "crypto/secure_hash.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace crypto {

std::array<uint8_t, kSHA256Length> SHA256Hash(base::span<const uint8_t> input) {
  std::array<uint8_t, kSHA256Length> digest;
  ::SHA256(input.data(), input.size(), digest.data());
  return digest;
}

void SHA256HashString(base::StringPiece str, void* output, size_t len) {
  std::unique_ptr<SecureHash> ctx(SecureHash::Create(SecureHash::SHA256));
  ctx->Update(str.data(), str.length());
  ctx->Finish(output, len);
}

std::string SHA256HashString(base::StringPiece str) {
  std::string output(kSHA256Length, 0);
  SHA256HashString(str, base::data(output), output.size());
  return output;
}

}  // namespace crypto
