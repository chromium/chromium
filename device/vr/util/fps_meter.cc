// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/util/fps_meter.h"

namespace device {

namespace {

static constexpr size_t kDefaultNumFrameTimes = 10;

}  // namespace

FPSMeter::FPSMeter() : frame_times_(kDefaultNumFrameTimes) {}

FPSMeter::FPSMeter(size_t window_size) : frame_times_(window_size) {}

FPSMeter::~FPSMeter() = default;

size_t FPSMeter::GetNumFrameTimes() {
  return frame_times_.GetWindowSize();
}

void FPSMeter::AddFrame(const base::TimeTicks& time_stamp) {
  if (last_time_stamp_.is_null()) {
    last_time_stamp_ = time_stamp;
    return;
  }

  int64_t delta = (time_stamp - last_time_stamp_).InMicroseconds();
  last_time_stamp_ = time_stamp;

  frame_times_.AddSample(delta);
}

bool FPSMeter::CanComputeFPS() const {
  return frame_times_.GetCount() > 0;
}

// Simply takes the average time delta.
double FPSMeter::GetFPS() const {
  if (!CanComputeFPS())
    return 0.0;

  return (frame_times_.GetCount() * 1.0e6) / frame_times_.GetSum();
}

}  // namespace device
