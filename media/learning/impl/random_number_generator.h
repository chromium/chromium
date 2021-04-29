// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_RANDOM_NUMBER_GENERATOR_H_
#define MEDIA_LEARNING_IMPL_RANDOM_NUMBER_GENERATOR_H_

#include <cstdint>
#include <memory>

#include "base/component_export.h"
#include "base/macros.h"

namespace media {

// Class to encapsulate a random number generator with an implementation for
// tests that provides repeatable, platform-independent sequences.
class COMPONENT_EXPORT(LEARNING_IMPL) RandomNumberGenerator {
 public:
  RandomNumberGenerator() = default;
  virtual ~RandomNumberGenerator() = default;

  // Return a random generator that will return unpredictable values in the
  // //base/rand_util.h sense.  See TestRandomGenerator if you'd like one that's
  // more predictable for tests.
  static RandomNumberGenerator* Default();

  // Taken from rand_util.h
  // Returns a random number in range [0, UINT64_MAX]. Thread-safe.
  virtual uint64_t Generate() = 0;

  // Returns a random number in range [0, range).  Thread-safe.
  uint64_t Generate(uint64_t range);

  // Returns a floating point number in the range [0, range).  Thread-safe.
  // This isn't an overload of Generate() to be sure that one isn't surprised by
  // the result.
  double GenerateDouble(double range);

 private:
  DISALLOW_COPY_AND_ASSIGN(RandomNumberGenerator);
};

// Handy mix-in class if you want to support rng injection.
class COMPONENT_EXPORT(LEARNING_IMPL) HasRandomNumberGenerator {
 public:
  // If |rng| is null, then we'll create a new one as a convenience.
  explicit HasRandomNumberGenerator(RandomNumberGenerator* rng = nullptr);
  ~HasRandomNumberGenerator();

  void SetRandomNumberGeneratorForTesting(RandomNumberGenerator* rng);

 protected:
  RandomNumberGenerator* rng() const { return rng_; }

 private:
  RandomNumberGenerator* rng_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_RANDOM_NUMBER_GENERATOR_H_
