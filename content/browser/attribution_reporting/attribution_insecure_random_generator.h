// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INSECURE_RANDOM_GENERATOR_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INSECURE_RANDOM_GENERATOR_H_

#include <stdint.h>

#include "content/browser/attribution_reporting/attribution_random_generator.h"

namespace absl {
class uint128;
}  // namespace absl

namespace content {

// A test-only insecure random number generator that uses XorShift128+
// internally.
class AttributionInsecureRandomGenerator : public AttributionRandomGenerator {
 public:
  explicit AttributionInsecureRandomGenerator(absl::uint128 seed);

  AttributionInsecureRandomGenerator(
      const AttributionInsecureRandomGenerator&) = delete;
  AttributionInsecureRandomGenerator(AttributionInsecureRandomGenerator&&) =
      delete;

  AttributionInsecureRandomGenerator& operator=(
      const AttributionInsecureRandomGenerator&) = delete;
  AttributionInsecureRandomGenerator& operator=(
      AttributionInsecureRandomGenerator&&) = delete;

  // AttributionStorageDelegateImpl::RandomGenerator:
  double RandDouble() override;
  int RandInt(int min, int max) override;
  void RandomShuffle(std::vector<AttributionReport>& reports) override;

 private:
  uint64_t RandUint64();

  // Returns a random number in range [0, range).
  uint64_t RandGenerator(uint64_t range);

  uint64_t state0_;
  uint64_t state1_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INSECURE_RANDOM_GENERATOR_H_
