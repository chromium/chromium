// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_number_generator.h"

#include <limits>

#include "base/rand_util.h"

namespace media {

class BaseRandomNumberGenerator : public RandomNumberGenerator {
 public:
  uint64_t Generate() override { return base::RandUint64(); }

 protected:
  ~BaseRandomNumberGenerator() override = default;
};

// static
RandomNumberGenerator* RandomNumberGenerator::Default() {
  static BaseRandomNumberGenerator* rng = nullptr;
  // TODO(liberato): locking?
  if (!rng)
    rng = new BaseRandomNumberGenerator();

  return rng;
}

uint64_t RandomNumberGenerator::Generate(uint64_t range) {
  // Don't just % generate(), since that wouldn't be uniform anymore.
  // This is copied from base/rand_util.cc .
  uint64_t max_acceptable_value =
      (std::numeric_limits<uint64_t>::max() / range) * range - 1;

  uint64_t value;
  do {
    value = Generate();
  } while (value > max_acceptable_value);

  return value % range;
}

double RandomNumberGenerator::GenerateDouble(double range) {
  return base::BitsToOpenEndedUnitInterval(Generate()) * range;
}

HasRandomNumberGenerator::HasRandomNumberGenerator(RandomNumberGenerator* rng)
    : rng_(rng ? rng : RandomNumberGenerator::Default()) {}

HasRandomNumberGenerator::~HasRandomNumberGenerator() = default;

void HasRandomNumberGenerator::SetRandomNumberGeneratorForTesting(
    RandomNumberGenerator* rng) {
  rng_ = rng;
}

}  // namespace media
