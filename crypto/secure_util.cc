// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/secure_util.h"

#include "third_party/boringssl/src/include/openssl/mem.h"

namespace crypto {

bool SecureMemEqual(const void* s1, const void* s2, size_t n) {
  return CRYPTO_memcmp(s1, s2, n) == 0;
}

}  // namespace crypto

