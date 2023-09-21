// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/scoped_raster_timer.h"

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace blink {

ScopedRasterTimer::ScopedRasterTimer(
    gpu::raster::RasterInterface* raster_interface,
    Host& host,
    bool always_measure_for_testing)
    : raster_interface_(raster_interface), host_(host) {
  // Subsample the RasterTimer metrics to reduce overhead.
  constexpr float kRasterMetricProbability = 0.01;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::MetricsSubSampler, metrics_subsampler,
                                  ());
  if (!metrics_subsampler.ShouldSample(kRasterMetricProbability) &&
      !always_measure_for_testing) {
    return;
  }

  active_ = true;  // Metric was activated by subsampler.
  if (raster_interface_) {
    host_.CheckGpuTimers(raster_interface_);
    gpu_timer_ = std::make_unique<AsyncGpuRasterTimer>(*raster_interface_);
  }
  timer_.emplace();
}

ScopedRasterTimer::~ScopedRasterTimer() {
  if (active_) {
    if (gpu_timer_) {
      gpu_timer_->FinishedIssuingCommands(*raster_interface_,
                                          timer_->Elapsed());
      host_.AddGpuTimer(std::move(gpu_timer_));
    } else {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          kRasterDurationUnacceleratedHistogram, timer_->Elapsed(),
          base::Microseconds(1), base::Milliseconds(100), 100);
    }
  }
}

// ScopedRasterTimer::AsyncGpuRasterTimer
//========================================

ScopedRasterTimer::AsyncGpuRasterTimer::AsyncGpuRasterTimer(
    gpu::raster::RasterInterface& raster_interface) {
  raster_interface.GenQueriesEXT(1, &gl_query_id_);
  raster_interface.BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, gl_query_id_);
}

void ScopedRasterTimer::AsyncGpuRasterTimer::FinishedIssuingCommands(
    gpu::raster::RasterInterface& raster_interface,
    base::TimeDelta cpu_raster_duration) {
  cpu_raster_duration_ = cpu_raster_duration;
  raster_interface.EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
}

bool ScopedRasterTimer::AsyncGpuRasterTimer::CheckTimer(
    gpu::raster::RasterInterface& raster_interface) {
  CHECK(!done_);
  raster_interface.GetQueryObjectuivEXT(
      gl_query_id_, GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT, &done_);
  if (done_) {
    GLuint raw_gpu_duration = 0u;
    raster_interface.GetQueryObjectuivEXT(gl_query_id_, GL_QUERY_RESULT_EXT,
                                          &raw_gpu_duration);
    base::TimeDelta gpu_duration_microseconds =
        base::Microseconds(raw_gpu_duration);
    base::TimeDelta total_time =
        gpu_duration_microseconds + cpu_raster_duration_;

    constexpr base::TimeDelta min = base::Microseconds(1);
    constexpr base::TimeDelta max = base::Milliseconds(100);
    constexpr int num_buckets = 100;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kRasterDurationAcceleratedGpuHistogram, gpu_duration_microseconds, min,
        max, num_buckets);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kRasterDurationAcceleratedCpuHistogram, cpu_raster_duration_, min, max,
        num_buckets);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        kRasterDurationAcceleratedTotalHistogram, total_time, min, max,
        num_buckets);

    raster_interface.DeleteQueriesEXT(1, &gl_query_id_);
  }
  return done_;
}

// ScopedRasterTimer::Host
//=========================

void ScopedRasterTimer::Host::CheckGpuTimers(
    gpu::raster::RasterInterface* raster_interface) {
  CHECK(raster_interface);
  WTF::EraseIf(gpu_timers_,
               [raster_interface](std::unique_ptr<AsyncGpuRasterTimer>& timer) {
                 return timer->CheckTimer(*raster_interface);
               });
}

void ScopedRasterTimer::Host::AddGpuTimer(
    std::unique_ptr<AsyncGpuRasterTimer> timer) {
  gpu_timers_.push_back(std::move(timer));
}

}  // namespace blink
