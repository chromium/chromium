// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_insecure_random_generator.h"

#include <stdint.h>

#include <limits>

#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace content {

namespace {

// Copied from `v8::base::RandomNumberGenerator::XorShift128()`.
void XorShift128(uint64_t* state0, uint64_t* state1) {
  uint64_t s1 = *state0;
  uint64_t s0 = *state1;
  *state0 = s0;
  s1 ^= s1 << 23;
  s1 ^= s1 >> 17;
  s1 ^= s0;
  s1 ^= s0 >> 26;
  *state1 = s1;
}

// Copied from `v8::base::RandomNumberGenerator::ToDouble()`.
double ToDouble(uint64_t state0) {
  // Exponent for double values for [1.0 .. 2.0)
  static const uint64_t kExponentBits = uint64_t{0x3FF0000000000000};
  uint64_t random = (state0 >> 12) | kExponentBits;
  return base::bit_cast<double>(random) - 1;
}

}  // namespace

AttributionInsecureRandomGenerator::AttributionInsecureRandomGenerator(
    absl::uint128 seed)
    : state0_(absl::Uint128Low64(seed)), state1_(absl::Uint128High64(seed)) {}

// Adapted from `v8::base::RandomNumberGenerator::NextInt64()`.
uint64_t AttributionInsecureRandomGenerator::RandUint64() {
  XorShift128(&state0_, &state1_);
  return state0_ + state1_;
}

// Copied from `v8::base::RandomNumberGenerator::ToDouble()`.
double AttributionInsecureRandomGenerator::RandDouble() {
  XorShift128(&state0_, &state1_);
  double result = ToDouble(state0_);
  DCHECK_GE(result, 0.0);
  DCHECK_LT(result, 1.0);
  return result;
}

// Adapted from `base::RandGenerator()`.
uint64_t AttributionInsecureRandomGenerator::RandGenerator(uint64_t range) {
  DCHECK_GT(range, 0u);
  // We must discard random results above this number, as they would
  // make the random generator non-uniform (consider e.g. if
  // MAX_UINT64 was 7 and |range| was 5, then a result of 1 would be twice
  // as likely as a result of 3 or 4).
  uint64_t max_acceptable_value =
      (std::numeric_limits<uint64_t>::max() / range) * range - 1;

  uint64_t value;
  do {
    value = RandUint64();
  } while (value > max_acceptable_value);

  return value % range;
}

// Adapted from `base::RandInt()`.
int AttributionInsecureRandomGenerator::RandInt(int min, int max) {
  DCHECK_LE(min, max);

  uint64_t range = static_cast<uint64_t>(max) - min + 1;
  // |range| is at most UINT_MAX + 1, so the result of `RandGenerator(range)`
  // is at most UINT_MAX.  Hence it's safe to cast it from uint64_t to int64_t.
  int result =
      static_cast<int>(min + static_cast<int64_t>(RandGenerator(range)));
  DCHECK_GE(result, min);
  DCHECK_LE(result, max);
  return result;
}

// Adapted from `base::RandomBitGenerator` and `base::RandomShuffle()`.
void AttributionInsecureRandomGenerator::RandomShuffle(
    std::vector<AttributionReport>& reports) {
  struct RandomBitGenerator {
    using result_type = uint64_t;
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() {
      return std::numeric_limits<uint64_t>::max();
    }
    result_type operator()() const { return gen->RandUint64(); }

    const raw_ref<AttributionInsecureRandomGenerator> gen;
  };

  base::ranges::shuffle(reports, RandomBitGenerator{.gen = raw_ref(*this)});
}

}  // namespace content
