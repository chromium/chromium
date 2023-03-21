// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/noiser_and_bucketer.h"

#include <stdint.h>

#include "base/rand_util.h"
#include "base/time/time.h"

namespace content {

namespace {

template <typename T>
T Noise(T input, int min, int max) {
  if (base::RandDouble() < 0.01) {
    return static_cast<T>(base::RandInt(min, max));
  }
  return input;
}

}  // namespace

uint8_t NoiseAndBucketJoinCount(int32_t join_count) {
  return Noise(internals::BucketJoinCount(join_count), 1, 16);
}

uint8_t NoiseAndBucketRecency(base::TimeDelta recency) {
  return Noise(internals::BucketRecency(recency), 0, 31);
}

uint16_t NoiseAndMaskModelingSignals(uint16_t modeling_signals) {
  constexpr uint16_t kMask = 0x0FFF;
  return Noise(modeling_signals & kMask, 0, kMask);
}

namespace internals {

uint8_t BucketJoinCount(int32_t join_count) {
  if (join_count < 1) {
    join_count = 1;
  }

  if (join_count <= 10) {
    return join_count;
  } else if (join_count <= 20) {
    return 11;
  } else if (join_count <= 30) {
    return 12;
  } else if (join_count <= 40) {
    return 13;
  } else if (join_count <= 50) {
    return 14;
  } else if (join_count <= 100) {
    return 15;
  }

  return 16;
}

uint8_t BucketRecency(base::TimeDelta recency) {
  if (recency < base::Minutes(0)) {
    recency = base::Minutes(0);
  }

  if (recency < base::Minutes(10)) {
    return recency.InMinutes();
  } else if (recency < base::Minutes(15)) {
    return 10;
  } else if (recency < base::Minutes(20)) {
    return 11;
  } else if (recency < base::Minutes(30)) {
    return 12;
  } else if (recency < base::Minutes(40)) {
    return 13;
  } else if (recency < base::Minutes(50)) {
    return 14;
  } else if (recency < base::Minutes(60)) {
    return 15;
  } else if (recency < base::Minutes(75)) {
    return 16;
  } else if (recency < base::Minutes(90)) {
    return 17;
  } else if (recency < base::Minutes(105)) {
    return 18;
  } else if (recency < base::Minutes(120)) {
    return 19;
  } else if (recency < base::Minutes(240)) {
    return 20;
  } else if (recency < base::Minutes(720)) {
    return 21;
  } else if (recency < base::Minutes(1440)) {
    return 22;
  } else if (recency < base::Minutes(2160)) {
    return 23;
  } else if (recency < base::Minutes(2880)) {
    return 24;
  } else if (recency < base::Minutes(4320)) {
    return 25;
  } else if (recency < base::Minutes(5760)) {
    return 26;
  } else if (recency < base::Minutes(10080)) {
    return 27;
  } else if (recency < base::Minutes(20160)) {
    return 28;
  } else if (recency < base::Minutes(30240)) {
    return 29;
  } else if (recency < base::Minutes(40320)) {
    return 30;
  }

  return 31;
}

}  // namespace internals

}  // namespace content
