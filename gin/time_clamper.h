// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_TIME_CLAMPER_H_
#define GIN_TIME_CLAMPER_H_

#include <algorithm>

#include "base/rand_util.h"
#include "base/time/time.h"
#include "gin/gin_export.h"

namespace gin {

// This class adds some amount of jitter to time. That is, for every
// `kResolutionMicros` microseconds it calculates a threshold (using a hash)
// that once exceeded advances to the next threshold. This is done so that
// time jumps slightly and does not move smoothly.
//
// NOTE: the implementation assumes it's used for servicing calls from JS,
// which uses the unix-epoch at time 0.
// TODO(skyostil): Deduplicate this with the clamper in Blink.
class GIN_EXPORT TimeClamper {
 public:
  // Public for tests.
  static const int64_t kResolutionMicros;

  TimeClamper() : secret_(base::RandUint64()) {}
  // This constructor should only be used in tests.
  explicit TimeClamper(uint64_t secret) : secret_(secret) {}

  TimeClamper(const TimeClamper&) = delete;
  TimeClamper& operator=(const TimeClamper&) = delete;
  ~TimeClamper() = default;

  // Clamps a time to millisecond precision. The return value is in milliseconds
  // relative to unix-epoch (which is what JS uses).
  inline int64_t ClampToMillis(base::Time time) const {
    // Adding jitter is non-trivial, only use it if necessary.
    // ClampTimeResolution() adjusts the time to land on `kResolutionMicros`
    // boundaries, and either uses the current `kResolutionMicros` boundary, or
    // the next one. Because `kResolutionMicros` is smaller than 1ms, and this
    // function returns millisecond accuracy, ClampTimeResolution() is only
    // necessary when within `kResolutionMicros` of the next millisecond.
    const int64_t now_micros =
        (time - base::Time::UnixEpoch()).InMicroseconds();
    const int64_t micros = now_micros % 1000;
    // abs() is necessary for devices with times before unix-epoch (most likely
    // configured incorrectly).
    if (abs(micros) + kResolutionMicros < 1000) {
      return now_micros / 1000;
    }
    return ClampTimeResolution(now_micros) / 1000;
  }

  // Clamps the time, giving microsecond precision. The return value is in
  // milliseconds relative to unix-epoch (which is what JS uses).
  inline double ClampToMillisHighResolution(base::Time now) const {
    const int64_t clamped_time =
        ClampTimeResolution((now - base::Time::UnixEpoch()).InMicroseconds());
    return static_cast<double>(clamped_time) / 1000.0;
  }

 private:
  inline int64_t ClampTimeResolution(int64_t time_micros) const {
    if (time_micros < 0) {
      return -ClampTimeResolutionPositiveValue(-time_micros);
    }
    return ClampTimeResolutionPositiveValue(time_micros);
  }

  inline int64_t ClampTimeResolutionPositiveValue(int64_t time_micros) const {
    DCHECK_GE(time_micros, 0u);
    // For each clamped time interval, compute a pseudorandom transition
    // threshold. The reported time will either be the start of that interval or
    // the next one depending on which side of the threshold |time_seconds| is.
    const int64_t interval = time_micros / kResolutionMicros;
    const int64_t clamped_time_micros = interval * kResolutionMicros;
    const int64_t tick_threshold = ThresholdFor(clamped_time_micros);
    if (time_micros - clamped_time_micros < tick_threshold) {
      return clamped_time_micros;
    }
    return clamped_time_micros + kResolutionMicros;
  }

  inline int64_t ThresholdFor(int64_t clamped_time) const {
    // Returns a random value between 0 and kResolutionMicros. The distribution
    // is not necessarily equal, but for a random value it's good enough.
    // Avoid floating-point math by rewriting:
    //   (random_value * 1.0 / UINT64_MAX) * kResolutionMicros
    // into:
    //   random_value / (UINT64_MAX / kResolutionMicros)
    // where we avoid integer overflow by dividing instead of multiplying.
    const uint64_t random_value = MurmurHash3(clamped_time ^ secret_);
    return std::min(static_cast<int64_t>(random_value /
                                         (std::numeric_limits<uint64_t>::max() /
                                          kResolutionMicros)),
                    kResolutionMicros);
  }

  static inline uint64_t MurmurHash3(uint64_t value) {
    value ^= value >> 33;
    value *= uint64_t{0xFF51AFD7ED558CCD};
    value ^= value >> 33;
    value *= uint64_t{0xC4CEB9FE1A85EC53};
    value ^= value >> 33;
    return value;
  }

  const uint64_t secret_;
};

}  // namespace gin

#endif  // GIN_TIME_CLAMPER_H_
