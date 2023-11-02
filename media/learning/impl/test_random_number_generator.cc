// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/test_random_number_generator.h"

namespace media {

TestRandomNumberGenerator::TestRandomNumberGenerator(uint32_t seed) {
  seed_ = seed & 0x7fffffff;  // make this a non-negative number
  if (seed_ == 0 || seed_ == M) {
    seed_ = 1;
  }
}

TestRandomNumberGenerator::~TestRandomNumberGenerator() = default;

uint64_t TestRandomNumberGenerator::Generate() {
  static const uint64_t A = 16807;  // bits 14, 8, 7, 5, 2, 1, 0
  uint64_t result = seed_ = static_cast<int32_t>((seed_ * A) % M);
  result <<= 32;
  result |= seed_ = static_cast<int32_t>((seed_ * A) % M);
  return result;
}

}  // namespace media
