// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_UTIL_SLIDING_AVERAGE_H_
#define DEVICE_VR_UTIL_SLIDING_AVERAGE_H_

#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "device/vr/util/sample_queue.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_VR_UTIL) SlidingAverage {
 public:
  explicit SlidingAverage(size_t window_size);
  ~SlidingAverage();

  void AddSample(int64_t value);
  int64_t GetAverageOrDefault(int64_t default_value) const;
  int64_t GetAverage() const { return GetAverageOrDefault(0); }
  size_t GetCount() const { return values_.GetCount(); }

 private:
  SampleQueue values_;
  DISALLOW_COPY_AND_ASSIGN(SlidingAverage);
};

class COMPONENT_EXPORT(DEVICE_VR_UTIL) SlidingTimeDeltaAverage {
 public:
  explicit SlidingTimeDeltaAverage(size_t window_size);
  virtual ~SlidingTimeDeltaAverage();

  virtual void AddSample(base::TimeDelta value);
  base::TimeDelta GetAverageOrDefault(base::TimeDelta default_value) const;
  base::TimeDelta GetAverage() const {
    return GetAverageOrDefault(base::TimeDelta());
  }
  size_t GetCount() const { return sample_microseconds_.GetCount(); }

 private:
  SlidingAverage sample_microseconds_;
  DISALLOW_COPY_AND_ASSIGN(SlidingTimeDeltaAverage);
};

}  // namespace device

#endif  // DEVICE_VR_UTIL_SLIDING_AVERAGE_H_
