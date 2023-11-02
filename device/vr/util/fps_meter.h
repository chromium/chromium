// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_UTIL_FPS_METER_H_
#define DEVICE_VR_UTIL_FPS_METER_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "device/vr/util/sample_queue.h"

namespace device {

// Computes fps based on submitted frame times.
class COMPONENT_EXPORT(DEVICE_VR_UTIL) FPSMeter {
 public:
  FPSMeter();
  explicit FPSMeter(size_t window_size);

  FPSMeter(const FPSMeter&) = delete;
  FPSMeter& operator=(const FPSMeter&) = delete;

  ~FPSMeter();

  void AddFrame(const base::TimeTicks& time_stamp);

  bool CanComputeFPS() const;

  double GetFPS() const;

  // Get sliding window size for tests.
  size_t GetNumFrameTimes();

 private:
  SampleQueue frame_times_;
  base::TimeTicks last_time_stamp_;
};

}  // namespace device

#endif  // DEVICE_VR_UTIL_FPS_METER_H_
