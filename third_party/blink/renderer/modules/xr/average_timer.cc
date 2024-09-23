// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/average_timer.h"

namespace blink {

void AverageTimer::StartTimer() {
  start_ = base::TimeTicks::Now();
}

void AverageTimer::StopTimer() {
  total_time_ += (base::TimeTicks::Now() - start_);
  size_++;
}

base::TimeDelta AverageTimer::TakeAverageMicroseconds() {
  base::TimeDelta average =
      size_ == 0 ? base::TimeDelta() : total_time_ / size_;
  total_time_ = base::TimeDelta();
  size_ = 0;
  return average;
}
}  // namespace blink
