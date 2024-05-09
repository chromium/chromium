// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/random.h"

#include <stddef.h>

#include <vector>

#include "base/rand_util.h"

namespace crypto {

void RandBytes(base::span<uint8_t> bytes) {
  // base::RandBytes() is already strongly random, so this is just an alias for
  // it. If base needs a non-strong RNG function in the future, it will get a
  // different name.
  base::RandBytes(bytes);
}

std::vector<uint8_t> RandBytesAsVector(size_t length) {
  std::vector<uint8_t> result(length);
  RandBytes(result);
  return result;
}

}  // namespace crypto
