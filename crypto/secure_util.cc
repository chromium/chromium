// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/secure_util.h"

#include "third_party/boringssl/src/include/openssl/mem.h"

namespace crypto {

bool SecureMemEqual(base::span<const uint8_t> s1,
                    base::span<const uint8_t> s2) {
  return s1.size() == s2.size() &&
         CRYPTO_memcmp(s1.data(), s2.data(), s1.size()) == 0;
}

}  // namespace crypto

