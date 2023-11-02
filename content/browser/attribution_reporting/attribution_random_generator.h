// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RANDOM_GENERATOR_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RANDOM_GENERATOR_H_

#include <vector>

namespace content {

class AttributionReport;

// Provides randomness to an `AttributionStorageDelegateImpl`.
//
// TODO(apaseltiner): Consider reconciling this interface with the APIs exposed
// in `base/rand_util.h`. Perhaps just a `UniformRandomBitGenerator` and a suite
// of helper functions which provide `RandDouble()`, `RandInt()`, etc with the
// generator as input, like `base::ranges::shuffle()`.
class AttributionRandomGenerator {
 public:
  virtual ~AttributionRandomGenerator() = default;

  // Returns a random double in the range [0, 1).
  virtual double RandDouble() = 0;

  // Returns a random int in the range [min, max].
  virtual int RandInt(int min, int max) = 0;

  // Shuffles `reports` randomly.
  virtual void RandomShuffle(std::vector<AttributionReport>& reports) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RANDOM_GENERATOR_H_
