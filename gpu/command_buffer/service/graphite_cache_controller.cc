// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_cache_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"

namespace gpu::raster {
namespace {
constexpr base::TimeDelta kCleanupDelay = base::Seconds(5);
}

GraphiteCacheController::GraphiteCacheController(
    skgpu::graphite::Recorder* recorder,
    skgpu::graphite::Context* context)
    : recorder_(recorder),
      context_(context),
      timer_(std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE,
          kCleanupDelay,
          base::BindRepeating(&GraphiteCacheController::PerformCleanup,
                              AsWeakPtr()))) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GraphiteCacheController::~GraphiteCacheController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GraphiteCacheController::ScheduleCleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cleanup resources which are not used in 5 seconds.
  constexpr std::chrono::seconds kNotUseTime{5};
  if (context_) {
    context_->performDeferredCleanup(kNotUseTime);
  }
  if (recorder_) {
    recorder_->performDeferredCleanup(kNotUseTime);
  }
  // Reset the timer, so PerformCleanup() will be called until ScheduleCleanup()
  // is not called for 5 seconds.
  timer_->Reset();
}

void GraphiteCacheController::PerformCleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cleanup all unused resources.
  if (context_) {
    context_->freeGpuResources();
  }
  if (recorder_) {
    recorder_->freeGpuResources();
  }
}

}  // namespace gpu::raster
