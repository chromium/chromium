// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/moving_average.h"

#include <algorithm>
#include <cmath>

#include "base/check_op.h"

namespace media {

MovingAverage::MovingAverage(size_t depth) : depth_(depth), samples_(depth_) {}

MovingAverage::~MovingAverage() = default;

void MovingAverage::AddSample(base::TimeDelta sample) {
  // |samples_| is zero-initialized, so |oldest| is also zero before |count_|
  // exceeds |depth_|.
  base::TimeDelta& oldest = samples_[count_++ % depth_];
  total_ += sample - oldest;
  oldest = sample;
  if (sample > max_)
    max_ = sample;
}

base::TimeDelta MovingAverage::Average() const {
  DCHECK_GT(count_, 0u);

  // TODO(dalecurtis): Consider limiting |depth| to powers of two so that we can
  // replace the integer divide with a bit shift operation.

  return total_ / std::min(static_cast<uint64_t>(depth_), count_);
}

base::TimeDelta MovingAverage::Deviation() const {
  DCHECK_GT(count_, 0u);
  const base::TimeDelta average = Average();
  const uint64_t size = std::min(static_cast<uint64_t>(depth_), count_);

  // Perform the calculation in floating point since squaring the delta can
  // exceed the bounds of a uint64_t value given two int64_t inputs.
  double deviation_secs = 0;
  for (uint64_t i = 0; i < size; ++i) {
    const double x = (samples_[i] - average).InSecondsF();
    deviation_secs += x * x;
  }

  deviation_secs /= size;
  return base::Seconds(std::sqrt(deviation_secs));
}

void MovingAverage::Reset() {
  count_ = 0;
  total_ = base::TimeDelta();
  max_ = kNoTimestamp;
  std::fill(samples_.begin(), samples_.end(), base::TimeDelta());
}

std::pair<base::TimeDelta, base::TimeDelta> MovingAverage::GetMinAndMax() {
  std::pair<base::TimeDelta, base::TimeDelta> result(samples_[0], samples_[0]);

  const uint64_t size = std::min(static_cast<uint64_t>(depth_), count_);
  for (uint64_t i = 1; i < size; i++) {
    if (samples_[i] < result.first)
      result.first = samples_[i];
    if (samples_[i] > result.second)
      result.second = samples_[i];
  }

  return result;
}

}  // namespace media
