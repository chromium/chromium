// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOVING_AVERAGE_H_
#define MEDIA_BASE_MOVING_AVERAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"

namespace media {

// Simple class for calculating a moving average of fixed size.
class MEDIA_EXPORT MovingAverage {
 public:
  // Creates a MovingAverage instance with space for |depth| samples.
  explicit MovingAverage(size_t depth);
  ~MovingAverage();

  // Adds a new sample to the average; replaces the oldest sample if |depth_|
  // has been exceeded.  Updates |total_| to the new sum of values.
  void AddSample(base::TimeDelta sample);

  // Returns the current average of all held samples.
  base::TimeDelta Average() const;

  // Returns the population standard deviation of all held samples.
  base::TimeDelta Deviation() const;

  // Resets the state of the class to its initial post-construction state.
  void Reset();

  uint64_t count() const { return count_; }

  base::TimeDelta max() const { return max_; }

 private:
  // Maximum number of elements allowed in the average.
  const size_t depth_;
  std::vector<base::TimeDelta> samples_;

  // Number of elements seen thus far.
  uint64_t count_ = 0;

  base::TimeDelta total_;

  // Maximum value ever seen.
  base::TimeDelta max_ = kNoTimestamp;

  DISALLOW_COPY_AND_ASSIGN(MovingAverage);
};

}  // namespace media

#endif  // MEDIA_BASE_MOVING_AVERAGE_H_
