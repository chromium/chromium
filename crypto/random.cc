// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/random.h"

#include <stddef.h>

#include <vector>

#include "base/rand_util.h"

namespace crypto {

void RandBytes(void *bytes, size_t length) {
  // It's OK to call base::RandBytes(), because it's already strongly random.
  // But _other_ code should go through this function to ensure that code which
  // needs secure randomness is easily discoverable.
  base::RandBytes(bytes, length);
}

void RandBytes(base::span<uint8_t> bytes) {
  RandBytes(bytes.data(), bytes.size());
}

std::vector<uint8_t> RandBytesAsVector(size_t length) {
  std::vector<uint8_t> result(length);
  RandBytes(result);
  return result;
}

}  // namespace crypto

