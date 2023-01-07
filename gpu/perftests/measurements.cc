// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/perftests/measurements.h"

#include <stdint.h>

#include "base/logging.h"
#include "testing/perf/perf_result_reporter.h"
#include "ui/gl/gpu_timing.h"

namespace gpu {

Measurement::Measurement() = default;
Measurement::Measurement(const Measurement& m) = default;
Measurement::Measurement(const std::string& metric_basename,
                         const base::TimeDelta wall_time,
                         const base::TimeDelta cpu_time,
                         const base::TimeDelta gpu_time)
    : metric_basename(metric_basename),
      wall_time(wall_time),
      cpu_time(cpu_time),
      gpu_time(gpu_time) {}

void Measurement::PrintResult(const std::string& story) const {
  auto reporter =
      std::make_unique<perf_test::PerfResultReporter>(metric_basename, story);
  reporter->RegisterImportantMetric("_wall", "ms");
  reporter->AddResult("_wall", wall_time.InMillisecondsF());
  if (cpu_time.InMicroseconds() >= 0) {
    reporter->RegisterImportantMetric("_cpu", "ms");
    reporter->AddResult("_cpu", cpu_time.InMillisecondsF());
  }
  if (gpu_time.InMicroseconds() >= 0) {
    reporter->RegisterImportantMetric("_gpu", "ms");
    reporter->AddResult("_gpu", gpu_time.InMillisecondsF());
  }
}

Measurement& Measurement::Increment(const Measurement& m) {
  wall_time += m.wall_time;
  cpu_time += m.cpu_time;
  gpu_time += m.gpu_time;
  return *this;
}

Measurement Measurement::Divide(int a) const {
  return Measurement(metric_basename, wall_time / a, cpu_time / a,
                     gpu_time / a);
}

Measurement::~Measurement() = default;

MeasurementTimers::MeasurementTimers(gl::GPUTimingClient* gpu_timing_client)
    : wall_time_start_(), cpu_time_start_(), gpu_timer_() {
  DCHECK(gpu_timing_client);
  wall_time_start_ = base::TimeTicks::Now();
  if (base::ThreadTicks::IsSupported()) {
    base::ThreadTicks::WaitUntilInitialized();
    cpu_time_start_ = base::ThreadTicks::Now();
  } else {
    static bool logged_once = false;
    LOG_IF(WARNING, !logged_once) << "ThreadTicks not supported.";
    logged_once = true;
  }

  if (gpu_timing_client->IsAvailable()) {
    gpu_timer_ = gpu_timing_client->CreateGPUTimer(true);
    gpu_timer_->Start();
  }
}

void MeasurementTimers::Record() {
  wall_time_ = base::TimeTicks::Now() - wall_time_start_;
  if (base::ThreadTicks::IsSupported()) {
    cpu_time_ = base::ThreadTicks::Now() - cpu_time_start_;
  }
  if (gpu_timer_.get()) {
    gpu_timer_->End();
  }
}

Measurement MeasurementTimers::GetAsMeasurement(
    const std::string& metric_basename) {
  DCHECK_NE(base::TimeDelta(),
            wall_time_);  // At least wall_time_ has been set.

  if (!base::ThreadTicks::IsSupported()) {
    cpu_time_ = base::Microseconds(-1);
  }
  int64_t gpu_time = -1;
  if (gpu_timer_.get() != nullptr && gpu_timer_->IsAvailable()) {
    gpu_time = gpu_timer_->GetDeltaElapsed();
  }
  return Measurement(metric_basename, wall_time_, cpu_time_,
                     base::Microseconds(gpu_time));
}

MeasurementTimers::~MeasurementTimers() {
  if (gpu_timer_.get()) {
    gpu_timer_->Destroy(true);
  }
}

}  // namespace gpu
