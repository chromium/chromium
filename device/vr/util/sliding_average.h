// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_UTIL_SLIDING_AVERAGE_H_
#define DEVICE_VR_UTIL_SLIDING_AVERAGE_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "device/vr/util/sample_queue.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_VR_UTIL) SlidingAverage {
 public:
  explicit SlidingAverage(size_t window_size);

  SlidingAverage(const SlidingAverage&) = delete;
  SlidingAverage& operator=(const SlidingAverage&) = delete;

  ~SlidingAverage();

  void AddSample(int64_t value);
  int64_t GetAverageOrDefault(int64_t default_value) const;
  int64_t GetAverage() const { return GetAverageOrDefault(0); }
  size_t GetCount() const { return values_.GetCount(); }

 private:
  SampleQueue values_;
};

class COMPONENT_EXPORT(DEVICE_VR_UTIL) SlidingTimeDeltaAverage {
 public:
  explicit SlidingTimeDeltaAverage(size_t window_size);

  SlidingTimeDeltaAverage(const SlidingTimeDeltaAverage&) = delete;
  SlidingTimeDeltaAverage& operator=(const SlidingTimeDeltaAverage&) = delete;

  virtual ~SlidingTimeDeltaAverage();

  virtual void AddSample(base::TimeDelta value);
  base::TimeDelta GetAverageOrDefault(base::TimeDelta default_value) const;
  base::TimeDelta GetAverage() const {
    return GetAverageOrDefault(base::TimeDelta());
  }
  size_t GetCount() const { return sample_microseconds_.GetCount(); }

 private:
  SlidingAverage sample_microseconds_;
};

}  // namespace device

#endif  // DEVICE_VR_UTIL_SLIDING_AVERAGE_H_
