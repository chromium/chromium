// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/time_clamper.h"

#include "base/bit_cast.h"
#include "base/rand_util.h"

#include <cmath>

namespace blink {

namespace {
const int64_t kTenLowerDigitsMod = 10000000000;
}  // namespace

TimeClamper::TimeClamper() : secret_(base::RandUint64()) {}

// This is using int64 for timestamps, because https://bit.ly/doubles-are-bad
base::TimeDelta TimeClamper::ClampTimeResolution(
    base::TimeDelta time,
    bool cross_origin_isolated_capability) const {
  int64_t time_microseconds = time.InMicroseconds();
  bool was_negative = false;

  // If the input time is negative, turn it to a positive one and keep track of
  // that.
  if (time_microseconds < 0) {
    was_negative = true;
    time_microseconds = -time_microseconds;
  }

  // Split the time_microseconds to lower and upper digits to prevent uniformity
  // distortion in large numbers. We will clamp the lower digits portion and
  // later add on the upper digits portion.
  int64_t time_lower_digits = time_microseconds % kTenLowerDigitsMod;
  int64_t time_upper_digits = time_microseconds - time_lower_digits;

  // Determine resolution based on the context's cross-origin isolation
  // capability. https://w3c.github.io/hr-time/#dfn-coarsen-time
  int resolution = cross_origin_isolated_capability
                       ? kFineResolutionMicroseconds
                       : kCoarseResolutionMicroseconds;

  // Clamped the time based on the resolution.
  int64_t clamped_time = time_lower_digits - time_lower_digits % resolution;

  // Determine if the clamped number should be clamped up, rather than down.
  // The threshold to determine that is a random number smaller than resolution,
  // such that the probability of clamped time being clamped up rather than
  // down is proportional to its distance from the clamped_down time.
  // As such it remains a double, in order to guarantee that distribution,
  // and the clamping's uniformity.
  double tick_threshold = ThresholdFor(clamped_time, resolution);
  if (time_lower_digits >= tick_threshold)
    clamped_time += resolution;

  // Add back the upper digits portion.
  clamped_time += time_upper_digits;

  // Flip the number back to being negative if it started that way.
  if (was_negative)
    clamped_time = -clamped_time;
  return base::Microseconds(clamped_time);
}

inline double TimeClamper::ThresholdFor(int64_t clamped_time,
                                        int resolution) const {
  uint64_t time_hash = MurmurHash3(clamped_time ^ secret_);
  return clamped_time + resolution * ToDouble(time_hash);
}

// static
inline double TimeClamper::ToDouble(uint64_t value) {
  // Exponent for double values for [1.0 .. 2.0]
  static const uint64_t kExponentBits = uint64_t{0x3FF0000000000000};
  static const uint64_t kMantissaMask = uint64_t{0x000FFFFFFFFFFFFF};
  uint64_t random = (value & kMantissaMask) | kExponentBits;
  return base::bit_cast<double>(random) - 1;
}

// static
inline uint64_t TimeClamper::MurmurHash3(uint64_t value) {
  value ^= value >> 33;
  value *= uint64_t{0xFF51AFD7ED558CCD};
  value ^= value >> 33;
  value *= uint64_t{0xC4CEB9FE1A85EC53};
  value ^= value >> 33;
  return value;
}

}  // namespace blink
