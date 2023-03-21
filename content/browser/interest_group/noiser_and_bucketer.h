// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_NOISER_AND_BUCKETER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_NOISER_AND_BUCKETER_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// Converts `join_count` to a 4-bit value (16 buckets), and randomly applies
// noise.
//
// Returns values in the range 1-16, inclusive.
//
// Unlike for recency and modeling signals, the first bucket is 1, since this
// way, a join count of 1 goes into bucket 1, a join count of 2 goes into bucket
// 2, etc., up to and including 10.
//
// The amount of information given to reportWin() is still the same would be
// given in a 0-15 bucketing scheme.
CONTENT_EXPORT uint8_t NoiseAndBucketJoinCount(int32_t join_count);

// Converts `recency` to a 5-bit value (32 buckets), and randomly applies
// noise.
//
// Returns values in the range 0-31, inclusive.
CONTENT_EXPORT uint8_t NoiseAndBucketRecency(base::TimeDelta recency);

// Masks `modeling_signals` to a 12-bit value (4096 values), and randomly
// applies noise.
//
// Returns values in the range 0-4095, inclusive.
CONTENT_EXPORT uint16_t NoiseAndMaskModelingSignals(uint16_t modeling_signals);

// Non-noised functions are exposed for tests only.
namespace internals {

CONTENT_EXPORT uint8_t BucketJoinCount(int32_t join_count);
CONTENT_EXPORT uint8_t BucketRecency(base::TimeDelta recency);

}  // namespace internals

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_NOISER_AND_BUCKETER_H_
