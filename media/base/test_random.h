// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEST_RANDOM_H_
#define MEDIA_BASE_TEST_RANDOM_H_

#include <stdint.h>

#include "base/check_op.h"

// Vastly simplified ACM random class meant to only be used for testing.
// This class is meant to generate predictable sequences of pseudorandom
// numbers, unlike the classes in base/rand_util.h which are meant to generate
// unpredictable sequences.
// See
// https://code.google.com/p/szl/source/browse/trunk/src/utilities/acmrandom.h
// for more information.

namespace media {

class TestRandom {
 public:
  explicit TestRandom(uint32_t seed) {
    seed_ = seed & 0x7fffffff;  // make this a non-negative number
    if (seed_ == 0 || seed_ == M) {
      seed_ = 1;
    }
  }

  int32_t Rand() {
    static const uint64_t A = 16807;  // bits 14, 8, 7, 5, 2, 1, 0
    seed_ = static_cast<int32_t>((seed_ * A) % M);
    CHECK_GT(seed_, 0);
    return seed_;
  }

 private:
  static const uint64_t M = 2147483647L;  // 2^32-1
  int32_t seed_;
};

}  // namespace media

#endif  // MEDIA_BASE_TEST_RANDOM_H_
