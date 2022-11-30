// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_TEST_RANDOM_NUMBER_GENERATOR_H_
#define MEDIA_LEARNING_IMPL_TEST_RANDOM_NUMBER_GENERATOR_H_

#include "media/learning/impl/random_number_generator.h"

namespace media {

// RandomGenerator implementation that provides repeatable (given a seed)
// sequences of numbers that is also platform agnostic.
class TestRandomNumberGenerator : public RandomNumberGenerator {
 public:
  explicit TestRandomNumberGenerator(uint32_t seed);
  ~TestRandomNumberGenerator() override;

  // RandomGenerator
  uint64_t Generate() override;

  static const uint64_t M = 2147483647L;  // 2^32-1
  int32_t seed_;
};

}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_TEST_RANDOM_NUMBER_GENERATOR_H_
